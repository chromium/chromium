// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/test_elf_image_builder.h"

#include <cstring>
#include <type_traits>
#include <utility>

#include "base/bits.h"
#include "base/check.h"
#include "base/notreached.h"
#include "build/build_config.h"

#if __SIZEOF_POINTER__ == 4
using Dyn = Elf32_Dyn;
using Nhdr = Elf32_Nhdr;
using Shdr = Elf32_Shdr;
#else
using Dyn = Elf64_Dyn;
using Nhdr = Elf64_Nhdr;
using Shdr = Elf64_Shdr;
#endif

namespace base {

namespace {
// Sizes/alignments to use in the ELF image.
static constexpr size_t kPageSize = 4096;
static constexpr size_t kPhdrAlign = 0x4;
static constexpr size_t kNoteAlign = 0x4;
static constexpr size_t kLoadAlign = 0x1000;
static constexpr size_t kDynamicAlign = 0x4;
}  // namespace

struct TestElfImageBuilder::LoadSegment {
  Word flags;
  Word size;
};

TestElfImage::TestElfImage(std::vector<uint8_t> buffer, const void* elf_start)
    : buffer_(std::move(buffer)), elf_start_(elf_start) {}

TestElfImage::~TestElfImage() = default;

TestElfImage::TestElfImage(TestElfImage&&) = default;

TestElfImage& TestElfImage::operator=(TestElfImage&&) = default;

TestElfImageBuilder::TestElfImageBuilder(MappingType mapping_type)
    : mapping_type_(mapping_type) {}

TestElfImageBuilder::~TestElfImageBuilder() = default;

TestElfImageBuilder& TestElfImageBuilder::AddLoadSegment(Word flags,
                                                         size_t size) {
  load_segments_.push_back({flags, static_cast<Word>(size)});
  return *this;
}

TestElfImageBuilder& TestElfImageBuilder::AddNoteSegment(
    Word type,
    StringPiece name,
    span<const uint8_t> desc) {
  const size_t name_with_null_size = name.size() + 1;
  std::vector<uint8_t> buffer(sizeof(Nhdr) +
                                  bits::AlignUp(name_with_null_size, 4) +
                                  bits::AlignUp(desc.size(), 4),
                              '\0');
  uint8_t* loc = &buffer.front();
  Nhdr* nhdr = reinterpret_cast<Nhdr*>(loc);
  nhdr->n_namesz = name_with_null_size;
  nhdr->n_descsz = desc.size();
  nhdr->n_type = type;
  loc += sizeof(Nhdr);

  memcpy(loc, name.data(), name.size());
  *(loc + name.size()) = '\0';
  loc += bits::AlignUp(name_with_null_size, 4);

  memcpy(loc, &desc.front(), desc.size());
  loc += bits::AlignUp(desc.size(), 4);

  DCHECK_EQ(&buffer.front() + buffer.size(), loc);

  note_contents_.push_back(std::move(buffer));

  return *this;
}

TestElfImageBuilder& TestElfImageBuilder::AddSoName(StringPiece soname) {
  DCHECK(!soname_.has_value());
  soname_.emplace(soname);
  return *this;
}

struct TestElfImageBuilder::ImageMeasures {
  size_t phdrs_required;
  size_t note_start;
  size_t note_size;
  std::vector<size_t> load_segment_start;
  size_t dynamic_start;
  size_t strtab_start;
  size_t total_size;
};

Addr TestElfImageBuilder::GetVirtualAddressForOffset(
    Off offset,
    const uint8_t* elf_start) const {
  switch (mapping_type_) {
    case RELOCATABLE:
      return static_cast<Addr>(offset);

    case RELOCATABLE_WITH_BIAS:
      return static_cast<Addr>(offset + kLoadBias);

    case NON_RELOCATABLE:
      return reinterpret_cast<Addr>(elf_start + offset);
  }
}

TestElfImageBuilder::ImageMeasures TestElfImageBuilder::MeasureSizesAndOffsets()
    const {
  ImageMeasures measures;

  measures.phdrs_required = 1 + load_segments_.size();
  if (!note_contents_.empty())
    ++measures.phdrs_required;
  if (soname_.has_value())
    ++measures.phdrs_required;

  // The current offset into the image, where the next bytes are to be written.
  // Starts after the ELF header.
  size_t offset = sizeof(Ehdr);

  // Add space for the program header table.
  offset = bits::AlignUp(offset, kPhdrAlign);
  offset += sizeof(Phdr) * measures.phdrs_required;

  // Add space for the notes.
  measures.note_start = offset;
  if (!note_contents_.empty())
    offset = bits::AlignUp(offset, kNoteAlign);
  for (const std::vector<uint8_t>& contents : note_contents_)
    offset += contents.size();
  measures.note_size = offset - measures.note_start;

  // Add space for the load segments.
  for (auto it = load_segments_.begin(); it != load_segments_.end(); ++it) {
    size_t size = 0;
    // The first non PT_PHDR program header is expected to be a PT_LOAD and
    // start at the already-aligned start of the ELF header.
    if (it == load_segments_.begin()) {
      size = offset + it->size;
      measures.load_segment_start.push_back(0);
    } else {
      offset = bits::AlignUp(offset, kLoadAlign);
      size = it->size;
      measures.load_segment_start.push_back(offset);
    }
    offset += it->size;
  }

  // Add space for the dynamic segment.
  measures.dynamic_start = bits::AlignUp(offset, kDynamicAlign);
  offset += sizeof(Dyn) * (soname_ ? 2 : 1);
  measures.strtab_start = offset;

  // Add space for the string table.
  ++offset;  // The first string table byte holds a null character.
  if (soname_)
    offset += soname_->size() + 1;

  measures.total_size = offset;

  return measures;
}

TestElfImage TestElfImageBuilder::Build() {
  ImageMeasures measures = MeasureSizesAndOffsets();

  // Write the ELF contents into |buffer|. Extends the buffer back to the 0
  // address in the case of load bias, so that the memory between the 0 address
  // and the image start is zero-initialized.
  const size_t load_bias =
      mapping_type_ == RELOCATABLE_WITH_BIAS ? kLoadBias : 0;
  std::vector<uint8_t> buffer(load_bias + (kPageSize - 1) + measures.total_size,
                              '\0');
  uint8_t* const elf_start =
      bits::AlignUp(&buffer.front() + load_bias, kPageSize);
  uint8_t* loc = elf_start;

  // Add the ELF header.
  loc = AppendHdr(CreateEhdr(measures.phdrs_required), loc);

  // Add the program header table.
  loc = bits::AlignUp(loc, kPhdrAlign);
  loc = AppendHdr(
      CreatePhdr(PT_PHDR, PF_R, kPhdrAlign, loc - elf_start,
                 GetVirtualAddressForOffset(loc - elf_start, elf_start),
                 sizeof(Phdr) * measures.phdrs_required),
      loc);
  for (size_t i = 0; i < load_segments_.size(); ++i) {
    const LoadSegment& load_segment = load_segments_[i];
    size_t size = load_segment.size;
    // The first non PT_PHDR program header is expected to be a PT_LOAD and
    // encompass all the preceding headers.
    if (i == 0)
      size += loc - elf_start;
    loc = AppendHdr(CreatePhdr(PT_LOAD, load_segment.flags, kLoadAlign,
                               measures.load_segment_start[i],
                               GetVirtualAddressForOffset(
                                   measures.load_segment_start[i], elf_start),
                               size),
                    loc);
  }
  if (measures.note_size != 0) {
    loc = AppendHdr(
        CreatePhdr(PT_NOTE, PF_R, kNoteAlign, measures.note_start,
                   GetVirtualAddressForOffset(measures.note_start, elf_start),
                   measures.note_size),
        loc);
  }
  if (soname_) {
    loc = AppendHdr(
        CreatePhdr(
            PT_DYNAMIC, PF_R | PF_W, kDynamicAlign, measures.dynamic_start,
            GetVirtualAddressForOffset(measures.dynamic_start, elf_start),
            sizeof(Dyn) * 2),
        loc);
  }

  // Add the notes.
  loc = bits::AlignUp(loc, kNoteAlign);
  for (const std::vector<uint8_t>& contents : note_contents_) {
    memcpy(loc, &contents.front(), contents.size());
    loc += contents.size();
  }

  // Add the load segments.
  for (auto it = load_segments_.begin(); it != load_segments_.end(); ++it) {
    if (it != load_segments_.begin())
      loc = bits::AlignUp(loc, kLoadAlign);
    memset(loc, 0, it->size);
    loc += it->size;
  }

  loc = bits::AlignUp(loc, kDynamicAlign);

  // Add the soname state.
  if (soname_) {
    // Add a DYNAMIC section for the soname.
    Dyn* soname_dyn = reinterpret_cast<Dyn*>(loc);
    soname_dyn->d_tag = DT_SONAME;
    soname_dyn->d_un.d_val = 1;  // One char into the string table.
    loc += sizeof(Dyn);
  }

  Dyn* strtab_dyn = reinterpret_cast<Dyn*>(loc);
  strtab_dyn->d_tag = DT_STRTAB;
#if defined(OS_FUCHSIA) || defined(OS_ANDROID)
  // Fuchsia and Android do not alter the symtab pointer on ELF load -- it's
  // expected to remain a 'virutal address'.
  strtab_dyn->d_un.d_ptr =
      GetVirtualAddressForOffset(measures.strtab_start, elf_start);
#else
  // Linux relocates this value on ELF load, so produce the pointer value after
  // relocation. That value will always be equal to the actual memory address.
  strtab_dyn->d_un.d_ptr =
      reinterpret_cast<uintptr_t>(elf_start + measures.strtab_start);
#endif
  loc += sizeof(Dyn);

  // Add a string table with one entry for the soname, if necessary.
  *loc++ = '\0';  // The first byte holds a null character.
  if (soname_) {
    memcpy(loc, soname_->data(), soname_->size());
    *(loc + soname_->size()) = '\0';
    loc += soname_->size() + 1;
  }

  // The offset past the end of the contents should be consistent with the size
  // mmeasurement above.
  DCHECK_EQ(loc, elf_start + measures.total_size);

  return TestElfImage(std::move(buffer), elf_start);
}

// static
template <typename T>
uint8_t* TestElfImageBuilder::AppendHdr(const T& hdr, uint8_t* loc) {
  static_assert(std::is_trivially_copyable<T>::value,
                "T should be a plain struct");
  memcpy(loc, &hdr, sizeof(T));
  return loc + sizeof(T);
}

Ehdr TestElfImageBuilder::CreateEhdr(Half phnum) {
  Ehdr ehdr;
  ehdr.e_ident[EI_MAG0] = ELFMAG0;
  ehdr.e_ident[EI_MAG1] = ELFMAG1;
  ehdr.e_ident[EI_MAG2] = ELFMAG2;
  ehdr.e_ident[EI_MAG3] = ELFMAG3;
  ehdr.e_ident[EI_CLASS] = __SIZEOF_POINTER__ == 4 ? 1 : 2;
  ehdr.e_ident[EI_DATA] = 1;  // Little endian.
  ehdr.e_ident[EI_VERSION] = 1;
  ehdr.e_ident[EI_OSABI] = 0x00;
  ehdr.e_ident[EI_ABIVERSION] = 0;
  ehdr.e_ident[EI_PAD] = 0;
  ehdr.e_type = ET_DYN;
  ehdr.e_machine = 0x28;  // ARM.
  ehdr.e_version = 1;
  ehdr.e_entry = 0;
  ehdr.e_phoff = sizeof(Ehdr);
  ehdr.e_shoff = 0;
  ehdr.e_flags = 0;
  ehdr.e_ehsize = sizeof(Ehdr);
  ehdr.e_phentsize = sizeof(Phdr);
  ehdr.e_phnum = phnum;
  ehdr.e_shentsize = sizeof(Shdr);
  ehdr.e_shnum = 0;
  ehdr.e_shstrndx = 0;

  return ehdr;
}

Phdr TestElfImageBuilder::CreatePhdr(Word type,
                                     Word flags,
                                     size_t align,
                                     Off offset,
                                     Addr vaddr,
                                     size_t size) {
  Phdr phdr;
  phdr.p_type = type;
  phdr.p_flags = flags;
  phdr.p_offset = offset;
  phdr.p_filesz = size;
  phdr.p_vaddr = vaddr;
  phdr.p_paddr = 0;
  phdr.p_memsz = phdr.p_filesz;
  phdr.p_align = align;

  return phdr;
}

}  // namespace base
