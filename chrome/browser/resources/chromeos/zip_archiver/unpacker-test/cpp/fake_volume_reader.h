// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_UNPACKER_TEST_CPP_FAKE_VOLUME_READER_H_
#define CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_UNPACKER_TEST_CPP_FAKE_VOLUME_READER_H_

#include "volume_reader.h"

// A fake VolumeReader for libarchive custom functions for processing archives
// data. The calls to VolumeReader are tested by integration tests as they are
// used only by libarchive. This class doesn't do anything and represents a
// dummy implementation of VolumeReader in order to isolate unit tests from
// VolumeReaderJavaScriptStream implementation.
class FakeVolumeReader : public VolumeReader {
 public:
  FakeVolumeReader();
  ~FakeVolumeReader() override;

  int64_t Read(int64_t bytes_to_read, const void** destination_buffer) override;
  int64_t Skip(int64_t bytes_to_skip);
  int64_t Seek(int64_t offset, base::File::Whence whence) override;
  std::unique_ptr<std::string> Passphrase() override;
};

#endif  // CHROME_BROWSER_RESOURCES_CHROMEOS_ZIP_ARCHIVER_UNPACKER_TEST_CPP_FAKE_VOLUME_READER_H_
