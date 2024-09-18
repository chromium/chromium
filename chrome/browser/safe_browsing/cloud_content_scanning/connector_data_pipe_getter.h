// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CONNECTOR_DATA_PIPE_GETTER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CONNECTOR_DATA_PIPE_GETTER_H_

#include <stdint.h>

#include <memory>

#include "base/compiler_specific.h"
#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/time/time.h"
#include "components/enterprise/obfuscation/core/download_obfuscator.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/network/public/mojom/data_pipe_getter.mojom.h"

namespace safe_browsing {

// This class implements mojom::DataPipeGetter for:
//
// 1. Multipart request with a body that has the following format:
// --BOUNDARY
// Content-Type: application/octet-stream
//
// <file metadata>
// --BOUNDARY
// Content-Type: application/octet-stream
//
// <file data>
// --BOUNDARY--
//
// 2. Resumable request with a body that contains solely file or page data.
class ConnectorDataPipeGetter : public network::mojom::DataPipeGetter {
 public:
#if BUILDFLAG(IS_POSIX)
  // Mimics base::MemoryMappedFile for READ_ONLY access.
  // The only difference between this class and base::MemoryMappedFile is that
  // if the mmap call fails using MAP_SHARED it will retry it with MAP_PRIVATE.
  // This is needed because some file systems (e.g., fuse in direct-io mode) do
  // not support MAP_SHARED. See crbug.com/1347488
  class InternalMemoryMappedFile {
   public:
    // The default constructor sets all members to invalid/null values.
    InternalMemoryMappedFile() = default;
    InternalMemoryMappedFile(const InternalMemoryMappedFile&) = delete;
    InternalMemoryMappedFile& operator=(const InternalMemoryMappedFile&) =
        delete;
    ~InternalMemoryMappedFile() { CloseHandles(); }

    [[nodiscard]] bool Initialize(base::File file);
    size_t length() const { return length_; }
    bool IsValid() const { return data_ != nullptr; }
    const uint8_t* data() const { return data_; }
    base::span<const uint8_t> bytes() const {
      // SAFETY: Class implementation maintains the invariant that `data_`
      // points to `length_` bytes of data.
      // TODO(https://crbug.com/40284755): Consider somehow reusing
      // `base::MemoryMappedFile::bytes` instead having to reimplement it
      // for the `#if BUILDFLAG(IS_POSIX)` fork of the class.
      return UNSAFE_BUFFERS(base::span(data_, length_));
    }

   private:
    bool DoInitialize();
    void CloseHandles();

    base::File file_;

    // RAW_PTR_EXCLUSION: Never allocated by PartitionAlloc (always mmap'ed), so
    // there is no benefit to using a raw_ptr, only cost.
    RAW_PTR_EXCLUSION uint8_t* data_ = nullptr;
    size_t length_ = 0;
  };
#else
  using InternalMemoryMappedFile = base::MemoryMappedFile;
#endif

  // Each constructor takes either a MemoryMappedFile representing an
  // uploaded/downloaded file, or a ReadOnlySharedMemoryMapping representing a
  // printed page. In both cases, the memory handle is assumed to be valid.
  ConnectorDataPipeGetter(const std::string& boundary,
                          const std::string& metadata,
                          std::unique_ptr<InternalMemoryMappedFile> file);
  ConnectorDataPipeGetter(const std::string& boundary,
                          const std::string& metadata,
                          base::ReadOnlySharedMemoryMapping page);
  ~ConnectorDataPipeGetter() override;

  // network::mojom::DataPipeGetter:
  void Read(mojo::ScopedDataPipeProducerHandle pipe,
            ReadCallback callback) override;
  void Clone(
      mojo::PendingReceiver<network::mojom::DataPipeGetter> receiver) override;

  // Static methods to create data pipe getter for multipart upload requests
  // with given boundary and metadata.
  //
  // Returns nullptr if `file` is invalid or if a memory mapped file can't be
  // created from it.
  static std::unique_ptr<ConnectorDataPipeGetter> CreateMultipartPipeGetter(
      const std::string& boundary,
      const std::string& metadata,
      base::File file);

  // Returns nullptr if `page` is invalid or if a memory region can't be created
  // from it.
  static std::unique_ptr<ConnectorDataPipeGetter> CreateMultipartPipeGetter(
      const std::string& boundary,
      const std::string& metadata,
      base::ReadOnlySharedMemoryRegion page);

  // Static methods to create data pipe getter for resumable upload requests
  // with no boundary or metadata needed.
  //
  // Returns nullptr if `file` is invalid or if a memory mapped file can't be
  // created from it.
  static std::unique_ptr<ConnectorDataPipeGetter> CreateResumablePipeGetter(
      base::File file);

  // Returns nullptr if `page` is invalid or if a memory region can't be created
  // from it.
  static std::unique_ptr<ConnectorDataPipeGetter> CreateResumablePipeGetter(
      base::ReadOnlySharedMemoryRegion page);

  // Resets `pipe_`, `watcher_`, and `write_position_` so future calls to Read
  // can work correctly.
  void Reset();

  // The file makes blocking calls when closing, so this method is used to
  // release it on a different thread so `this` can call its dtor immediately.
  std::unique_ptr<InternalMemoryMappedFile> ReleaseFile();

  // Accessors to `file_data_pipe_` to check if either `file_` or `page_` is
  // populated.
  bool is_file_data_pipe() const;
  bool is_page_data_pipe() const;

 private:
  // Private constructor that shares initialization logics across file and page
  // data pipes.
  ConnectorDataPipeGetter(const std::string& boundary,
                          const std::string& metadata,
                          std::unique_ptr<InternalMemoryMappedFile> file,
                          base::ReadOnlySharedMemoryMapping page);

  // Callback used by `watcher_`.
  void MojoReadyCallback(MojoResult result,
                         const mojo::HandleSignalsState& state);

  // Calls the appropriate Write method according to `write_position_`.
  void Write();

  // Prepares strings to be written at the start and the end of the body.
  void PrepareMultipartRequestFormat(const std::string& boundary,
                                     const std::string& metadata);

  // Methods to write a request format (string), a file or a page to `pipe_`.
  // Returns true if further Write methods can be called.
  bool WriteMultipartRequestFormat(const std::string& str, int64_t offset);
  bool WriteFileData();
  bool WritePageData();
  bool Write(base::span<const uint8_t> data);
  // Checks if `write_position_` is within the expected range.
  bool IsWritePositionInRange(int64_t range_start, int64_t range_end);

  // Returns the total size of the body generated by the class.
  int64_t FullSize();

  // Value to be written at the start of the body for multipart request.
  std::string metadata_;

  // Value to be written at the end of the body for multipart request.
  std::string last_boundary_;

  // This class uses a memory mapped file or memory mapping to avoid blocking
  // calls on the main thread. Only one of `file_` or `page_` will be populated
  // for a given data pipe getter.
  std::unique_ptr<InternalMemoryMappedFile> file_;

  base::ReadOnlySharedMemoryMapping page_;
  int64_t write_position_ = 0;

  // Indicates if this data pipe streams a pipe. If false, it streams a page.
  bool file_data_pipe_;

  std::unique_ptr<enterprise_obfuscation::DownloadObfuscator> deobfuscator_;

  mojo::ScopedDataPipeProducerHandle pipe_;
  std::unique_ptr<mojo::SimpleWatcher> watcher_;
  mojo::ReceiverSet<network::mojom::DataPipeGetter> receivers_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CLOUD_CONTENT_SCANNING_CONNECTOR_DATA_PIPE_GETTER_H_
