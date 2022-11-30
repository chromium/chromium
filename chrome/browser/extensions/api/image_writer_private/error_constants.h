// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ERROR_CONSTANTS_H_
#define CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ERROR_CONSTANTS_H_

namespace extensions {
namespace image_writer {
namespace error {

// General operation messages.
extern const char kAborted[];
extern const char kNoOperationInProgress[];
extern const char kOperationAlreadyInProgress[];
extern const char kUnsupportedOperation[];

// Device listing errors
extern const char kDeviceListError[];

// File errors
extern const char kInvalidFileEntry[];

// Download errors
extern const char kDownloadCancelled[];
extern const char kDownloadHashError[];
extern const char kDownloadInterrupted[];
extern const char kTempDirError[];
extern const char kTempFileError[];
extern const char kUrlInvalid[];

// Unzip errors
extern const char kUnzipGenericError[];
extern const char kUnzipInvalidArchive[];

// Write errors
extern const char kDeviceCloseError[];
extern const char kDeviceInvalid[];
extern const char kDeviceHashError[];
extern const char kDeviceOpenError[];
extern const char kDeviceWriteError[];
extern const char kImageCloseError[];
extern const char kImageInvalid[];
extern const char kImageHashError[];
extern const char kImageNotFound[];
extern const char kImageOpenError[];
extern const char kImageReadError[];
extern const char kImageSizeError[];
extern const char kUnmountVolumesError[];

// Verification Errors
extern const char kHashReadError[];
extern const char kVerificationFailed[];

// Image burner catchall
extern const char kChromeOSImageBurnerError[];

}  // namespace error
}  // namespace image_writer
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IMAGE_WRITER_PRIVATE_ERROR_CONSTANTS_H_
