// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_WIN_PE_IMAGE_READER_H_
#define BASE_WIN_PE_IMAGE_READER_H_

#include <stddef.h>
#include <stdint.h>
#include <windows.h>

#include <memory>

#include "base/base_export.h"
#include "base/macros.h"

namespace base {
namespace win {

// Parses headers and various data from a PE image. This parser is safe for use
// on untrusted data and works on PE files with different bitness from the
// current process. The PeImageReader is initialized after construction by
// passing the address and size of a PE file that has been read into memory -
// not loaded by the OS as an image. Parsing of a PE file that has been loaded
// as an image can be done with PEImage.
class BASE_EXPORT PeImageReader {
 public:
  enum WordSize {
    WORD_SIZE_32,
    WORD_SIZE_64,
  };

  // A callback invoked by EnumCertificates once for each attribute certificate
  // entry in the image's attribute certificate table. |revision| and
  // |certificate_type| identify the contents of |certificate_data| (which is of
  // |certificate_data_size| bytes). |context| is the value provided by the
  // caller to EnumCertificates(). Implementations must return true to continue
  // the enumeration, or false to abort.
  using EnumCertificatesCallback = bool (*)(uint16_t revision,
                                            uint16_t certificate_type,
                                            const uint8_t* certificate_data,
                                            size_t certificate_data_size,
                                            void* context);

  PeImageReader();
  ~PeImageReader();

  // Returns false if the given data does not appear to be a valid PE image.
  bool Initialize(const uint8_t* image_data, size_t image_size);

  // Returns the machine word size for the image.
  WordSize GetWordSize();

  const IMAGE_DOS_HEADER* GetDosHeader();
  const IMAGE_FILE_HEADER* GetCoffFileHeader();

  // Returns a pointer to the optional header and its size.
  const uint8_t* GetOptionalHeaderData(size_t* optional_data_size);
  size_t GetNumberOfSections();
  const IMAGE_SECTION_HEADER* GetSectionHeaderAt(size_t index);

  // Returns a pointer to the image's export data (.edata) section and its size,
  // or nullptr if the section is not present.
  const uint8_t* GetExportSection(size_t* section_size);

  size_t GetNumberOfDebugEntries();
  const IMAGE_DEBUG_DIRECTORY* GetDebugEntry(size_t index,
                                             const uint8_t** raw_data,
                                             size_t* raw_data_size);

  // Invokes |callback| once per attribute certificate entry. |context| is a
  // caller-specific value that is passed to |callback|. Returns true if all
  // certificate entries are visited (even if there are no such entries) and
  // |callback| returns true for each. Conversely, returns |false| if |callback|
  // returns false or if the image is malformed in any way.
  bool EnumCertificates(EnumCertificatesCallback callback, void* context);

  // Returns the size of the image file.
  DWORD GetSizeOfImage();

 private:
  // Bits indicating what portions of the image have been validated.
  enum ValidationStages {
    VALID_DOS_HEADER = 1 << 0,
    VALID_PE_SIGNATURE = 1 << 1,
    VALID_COFF_FILE_HEADER = 1 << 2,
    VALID_OPTIONAL_HEADER = 1 << 3,
    VALID_SECTION_HEADERS = 1 << 4,
  };

  // An interface to an image's optional header.
  class OptionalHeader {
   public:
    virtual ~OptionalHeader() = default;

    virtual WordSize GetWordSize() = 0;

    // Returns the offset of the DataDirectory member relative to the start of
    // the optional header.
    virtual size_t GetDataDirectoryOffset() = 0;

    // Returns the number of entries in the data directory.
    virtual DWORD GetDataDirectorySize() = 0;

    // Returns a pointer to the first data directory entry.
    virtual const IMAGE_DATA_DIRECTORY* GetDataDirectoryEntries() = 0;

    // Returns the size of the image file.
    virtual DWORD GetSizeOfImage() = 0;
  };

  template <class OPTIONAL_HEADER_TYPE>
  class OptionalHeaderImpl;

  void Clear();
  bool ValidateDosHeader();
  bool ValidatePeSignature();
  bool ValidateCoffFileHeader();
  bool ValidateOptionalHeader();
  bool ValidateSectionHeaders();

  // Return a pointer to the first byte of the image's optional header.
  const uint8_t* GetOptionalHeaderStart();
  size_t GetOptionalHeaderSize();

  // Returns the desired directory entry, or nullptr if |index| is out of
  // bounds.
  const IMAGE_DATA_DIRECTORY* GetDataDirectoryEntryAt(size_t index);

  // Returns the header for the section that contains the given address, or
  // nullptr if the address is out of bounds or the image does not contain the
  // section.
  const IMAGE_SECTION_HEADER* FindSectionFromRva(uint32_t relative_address);

  // Returns a pointer to the |data_length| bytes referenced by the |index|'th
  // data directory entry.
  const uint8_t* GetImageData(size_t index, size_t* data_length);

  // Populates |structure| with a pointer to a desired structure of type T at
  // the given offset if the image is sufficiently large to contain it. Returns
  // false if the structure does not fully fit within the image at the given
  // offset.
  template <typename T>
  bool GetStructureAt(size_t offset, const T** structure) {
    return GetStructureAt(offset, sizeof(**structure), structure);
  }

  // Populates |structure| with a pointer to a desired structure of type T at
  // the given offset if the image is sufficiently large to contain
  // |structure_size| bytes. Returns false if the structure does not fully fit
  // within the image at the given offset.
  template <typename T>
  bool GetStructureAt(size_t offset,
                      size_t structure_size,
                      const T** structure) {
    if (offset > image_size_)
      return false;
    if (structure_size > image_size_ - offset)
      return false;
    *structure = reinterpret_cast<const T*>(image_data_ + offset);
    return true;
  }

  const uint8_t* image_data_ = nullptr;
  size_t image_size_ = 0;
  uint32_t validation_state_ = 0;
  std::unique_ptr<OptionalHeader> optional_header_;
  DISALLOW_COPY_AND_ASSIGN(PeImageReader);
};

}  // namespace win
}  // namespace base

#endif  // BASE_WIN_PE_IMAGE_READER_H_
