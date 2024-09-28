// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/win/pe_image_reader.h"

#include <wintrust.h>

#include <memory>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/numerics/safe_math.h"

namespace base {
namespace win {

// A class template of traits pertaining to IMAGE_OPTIONAL_HEADER{32,64}.
template <class HEADER_TYPE>
struct OptionalHeaderTraits {};

template <>
struct OptionalHeaderTraits<IMAGE_OPTIONAL_HEADER32> {
  static const PeImageReader::WordSize word_size = PeImageReader::WORD_SIZE_32;
};

template <>
struct OptionalHeaderTraits<IMAGE_OPTIONAL_HEADER64> {
  static const PeImageReader::WordSize word_size = PeImageReader::WORD_SIZE_64;
};

// A template for type-specific optional header implementations. This, in
// conjunction with the OptionalHeader interface, effectively erases the
// underlying structure type from the point of view of the PeImageReader.
template <class OPTIONAL_HEADER_TYPE>
class PeImageReader::OptionalHeaderImpl : public PeImageReader::OptionalHeader {
 public:
  using TraitsType = OptionalHeaderTraits<OPTIONAL_HEADER_TYPE>;

  explicit OptionalHeaderImpl(const uint8_t* optional_header_start)
      : optional_header_(reinterpret_cast<const OPTIONAL_HEADER_TYPE*>(
            optional_header_start)) {}

  OptionalHeaderImpl(const OptionalHeaderImpl&) = delete;
  OptionalHeaderImpl& operator=(const OptionalHeaderImpl&) = delete;

  WordSize GetWordSize() override { return TraitsType::word_size; }

  size_t GetDataDirectoryOffset() override {
    return offsetof(OPTIONAL_HEADER_TYPE, DataDirectory);
  }

  DWORD GetDataDirectorySize() override {
    return optional_header_->NumberOfRvaAndSizes;
  }

  const IMAGE_DATA_DIRECTORY* GetDataDirectoryEntries() override {
    return &optional_header_->DataDirectory[0];
  }

  DWORD GetSizeOfImage() override { return optional_header_->SizeOfImage; }

 private:
  raw_ptr<const OPTIONAL_HEADER_TYPE> optional_header_;
};

PeImageReader::PeImageReader() = default;

PeImageReader::~PeImageReader() {
  Clear();
}

bool PeImageReader::Initialize(span<const uint8_t> image_data) {
  image_data_ = image_data;

  if (!ValidateDosHeader() || !ValidatePeSignature() ||
      !ValidateCoffFileHeader() || !ValidateOptionalHeader() ||
      !ValidateSectionHeaders()) {
    Clear();
    return false;
  }

  return true;
}

PeImageReader::WordSize PeImageReader::GetWordSize() {
  return optional_header_->GetWordSize();
}

const IMAGE_DOS_HEADER* PeImageReader::GetDosHeader() {
  DCHECK_NE((validation_state_ & VALID_DOS_HEADER), 0U);
  return reinterpret_cast<const IMAGE_DOS_HEADER*>(image_data_.data());
}

const IMAGE_FILE_HEADER* PeImageReader::GetCoffFileHeader() {
  DCHECK_NE((validation_state_ & VALID_COFF_FILE_HEADER), 0U);
  return reinterpret_cast<const IMAGE_FILE_HEADER*>(
      image_data_
          .subspan(checked_cast<size_t>(GetDosHeader()->e_lfanew) +
                   sizeof(DWORD))
          .data());
}

span<const uint8_t> PeImageReader::GetOptionalHeaderData() {
  return make_span(GetOptionalHeaderStart(), GetOptionalHeaderSize());
}

size_t PeImageReader::GetNumberOfSections() {
  return GetCoffFileHeader()->NumberOfSections;
}

const IMAGE_SECTION_HEADER* PeImageReader::GetSectionHeaderAt(size_t index) {
  DCHECK_NE((validation_state_ & VALID_SECTION_HEADERS), 0U);
  DCHECK_LT(index, GetNumberOfSections());
  return reinterpret_cast<const IMAGE_SECTION_HEADER*>(
      GetOptionalHeaderStart() + GetOptionalHeaderSize() +
      (sizeof(IMAGE_SECTION_HEADER) * index));
}

span<const uint8_t> PeImageReader::GetExportSection() {
  span<const uint8_t> data = GetImageData(IMAGE_DIRECTORY_ENTRY_EXPORT);

  // The export section data must be big enough for the export directory.
  if (data.size() < sizeof(IMAGE_EXPORT_DIRECTORY)) {
    return span<const uint8_t>();
  }

  return data;
}

size_t PeImageReader::GetNumberOfDebugEntries() {
  return GetImageData(IMAGE_DIRECTORY_ENTRY_DEBUG).size() /
         sizeof(IMAGE_DEBUG_DIRECTORY);
}

const IMAGE_DEBUG_DIRECTORY* PeImageReader::GetDebugEntry(
    size_t index,
    span<const uint8_t>& raw_data) {
  DCHECK_LT(index, GetNumberOfDebugEntries());

  // Get the debug directory.
  span<const uint8_t> debug_directory_data =
      GetImageData(IMAGE_DIRECTORY_ENTRY_DEBUG);
  if (debug_directory_data.empty()) {
    return nullptr;
  }

  const IMAGE_DEBUG_DIRECTORY& entry =
      reinterpret_cast<const IMAGE_DEBUG_DIRECTORY&>(
          debug_directory_data[index * sizeof(IMAGE_DEBUG_DIRECTORY)]);
  const uint8_t* debug_data = nullptr;
  if (GetStructureAt(entry.PointerToRawData, entry.SizeOfData, &debug_data)) {
    raw_data = make_span(debug_data, entry.SizeOfData);
  }
  return &entry;
}

bool PeImageReader::EnumCertificates(EnumCertificatesCallback callback,
                                     void* context) {
  span<const uint8_t> data = GetImageData(IMAGE_DIRECTORY_ENTRY_SECURITY);
  const size_t kWinCertificateSize = offsetof(WIN_CERTIFICATE, bCertificate);
  while (!data.empty()) {
    const WIN_CERTIFICATE* win_certificate =
        reinterpret_cast<const WIN_CERTIFICATE*>(data.data());
    if (kWinCertificateSize > data.size() ||
        kWinCertificateSize > win_certificate->dwLength ||
        win_certificate->dwLength > data.size()) {
      return false;
    }
    if (!(*callback)(
            win_certificate->wRevision, win_certificate->wCertificateType,
            base::span(&win_certificate->bCertificate[0],
                       win_certificate->dwLength - kWinCertificateSize),
            context)) {
      return false;
    }
    size_t padded_length = (win_certificate->dwLength + 7) & ~0x7u;
    if (padded_length > data.size()) {
      return false;
    }
    data = data.subspan(padded_length);
  }
  return true;
}

DWORD PeImageReader::GetSizeOfImage() {
  return optional_header_->GetSizeOfImage();
}

void PeImageReader::Clear() {
  image_data_ = raw_span<const uint8_t>();
  validation_state_ = 0;
  optional_header_.reset();
}

bool PeImageReader::ValidateDosHeader() {
  const IMAGE_DOS_HEADER* dos_header = nullptr;
  if (!GetStructureAt(0, &dos_header) ||
      dos_header->e_magic != IMAGE_DOS_SIGNATURE || dos_header->e_lfanew < 0) {
    return false;
  }

  validation_state_ |= VALID_DOS_HEADER;
  return true;
}

bool PeImageReader::ValidatePeSignature() {
  const DWORD* signature = nullptr;
  if (!GetStructureAt(static_cast<size_t>(GetDosHeader()->e_lfanew),
                      &signature) ||
      *signature != IMAGE_NT_SIGNATURE) {
    return false;
  }

  validation_state_ |= VALID_PE_SIGNATURE;
  return true;
}

bool PeImageReader::ValidateCoffFileHeader() {
  DCHECK_NE((validation_state_ & VALID_PE_SIGNATURE), 0U);
  const IMAGE_FILE_HEADER* file_header = nullptr;
  if (!GetStructureAt(static_cast<size_t>(GetDosHeader()->e_lfanew) +
                          offsetof(IMAGE_NT_HEADERS32, FileHeader),
                      &file_header)) {
    return false;
  }

  validation_state_ |= VALID_COFF_FILE_HEADER;
  return true;
}

bool PeImageReader::ValidateOptionalHeader() {
  const IMAGE_FILE_HEADER* file_header = GetCoffFileHeader();
  const size_t optional_header_offset =
      static_cast<size_t>(GetDosHeader()->e_lfanew) +
      offsetof(IMAGE_NT_HEADERS32, OptionalHeader);
  const size_t optional_header_size = file_header->SizeOfOptionalHeader;
  const WORD* optional_header_magic = nullptr;

  if (optional_header_size < sizeof(*optional_header_magic) ||
      !GetStructureAt(optional_header_offset, &optional_header_magic)) {
    return false;
  }

  std::unique_ptr<OptionalHeader> optional_header;
  if (*optional_header_magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
    optional_header =
        std::make_unique<OptionalHeaderImpl<IMAGE_OPTIONAL_HEADER32>>(
            image_data_.subspan(optional_header_offset).data());
  } else if (*optional_header_magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    optional_header =
        std::make_unique<OptionalHeaderImpl<IMAGE_OPTIONAL_HEADER64>>(
            image_data_.subspan(optional_header_offset).data());
  } else {
    return false;
  }

  // Does all of the claimed optional header fit in the image?
  if (optional_header_size > image_data_.size() - optional_header_offset) {
    return false;
  }

  // Is the claimed optional header big enough for everything but the dir?
  if (optional_header->GetDataDirectoryOffset() > optional_header_size)
    return false;

  // Is there enough room for all of the claimed directory entries?
  if (optional_header->GetDataDirectorySize() >
      ((optional_header_size - optional_header->GetDataDirectoryOffset()) /
       sizeof(IMAGE_DATA_DIRECTORY))) {
    return false;
  }

  optional_header_.swap(optional_header);
  validation_state_ |= VALID_OPTIONAL_HEADER;
  return true;
}

bool PeImageReader::ValidateSectionHeaders() {
  const uint8_t* first_section_header =
      GetOptionalHeaderStart() + GetOptionalHeaderSize();
  const size_t number_of_sections = GetNumberOfSections();

  // Do all section headers fit in the image?
  if (!GetStructureAt(
          static_cast<size_t>(first_section_header - image_data_.data()),
          number_of_sections * sizeof(IMAGE_SECTION_HEADER),
          &first_section_header)) {
    return false;
  }

  validation_state_ |= VALID_SECTION_HEADERS;
  return true;
}

const uint8_t* PeImageReader::GetOptionalHeaderStart() {
  DCHECK_NE((validation_state_ & VALID_OPTIONAL_HEADER), 0U);
  return image_data_
      .subspan(checked_cast<size_t>(GetDosHeader()->e_lfanew) +
               offsetof(IMAGE_NT_HEADERS32, OptionalHeader))
      .data();
}

size_t PeImageReader::GetOptionalHeaderSize() {
  return GetCoffFileHeader()->SizeOfOptionalHeader;
}

const IMAGE_DATA_DIRECTORY* PeImageReader::GetDataDirectoryEntryAt(
    size_t index) {
  DCHECK_NE((validation_state_ & VALID_OPTIONAL_HEADER), 0U);
  if (index >= optional_header_->GetDataDirectorySize())
    return nullptr;
  return &optional_header_->GetDataDirectoryEntries()[index];
}

const IMAGE_SECTION_HEADER* PeImageReader::FindSectionFromRva(
    uint32_t relative_address) {
  const size_t number_of_sections = GetNumberOfSections();
  for (size_t i = 0; i < number_of_sections; ++i) {
    const IMAGE_SECTION_HEADER* section_header = GetSectionHeaderAt(i);
    // Is the raw data present in the image? If no, optimistically keep looking.
    const uint8_t* section_data = nullptr;
    if (!GetStructureAt(section_header->PointerToRawData,
                        section_header->SizeOfRawData, &section_data)) {
      continue;
    }
    // Does the RVA lie on or after this section's start when mapped? If no,
    // bail.
    if (section_header->VirtualAddress > relative_address)
      break;
    // Does the RVA lie within the section when mapped? If no, keep looking.
    size_t address_offset = relative_address - section_header->VirtualAddress;
    if (address_offset > section_header->Misc.VirtualSize)
      continue;
    // We have a winner.
    return section_header;
  }
  return nullptr;
}

span<const uint8_t> PeImageReader::GetImageData(size_t index) {
  // Get the requested directory entry.
  const IMAGE_DATA_DIRECTORY* entry = GetDataDirectoryEntryAt(index);
  if (!entry)
    return span<const uint8_t>();

  // The entry for the certificate table is special in that its address is a
  // file pointer rather than an RVA.
  if (index == IMAGE_DIRECTORY_ENTRY_SECURITY) {
    // Does the data fit within the file.
    if (entry->VirtualAddress > image_data_.size() ||
        image_data_.size() - entry->VirtualAddress < entry->Size) {
      return span<const uint8_t>();
    }
    return image_data_.subspan(entry->VirtualAddress, entry->Size);
  }

  // Find the section containing the data.
  const IMAGE_SECTION_HEADER* header =
      FindSectionFromRva(entry->VirtualAddress);
  if (!header)
    return span<const uint8_t>();

  // Does the data fit within the section when mapped?
  size_t data_offset = entry->VirtualAddress - header->VirtualAddress;
  if (entry->Size > (header->Misc.VirtualSize - data_offset))
    return span<const uint8_t>();

  // Is the data entirely present on disk (if not it's zeroed out when loaded)?
  if (data_offset >= header->SizeOfRawData ||
      header->SizeOfRawData - data_offset < entry->Size) {
    return span<const uint8_t>();
  }

  return image_data_.subspan(header->PointerToRawData + data_offset,
                             entry->Size);
}

}  // namespace win
}  // namespace base
