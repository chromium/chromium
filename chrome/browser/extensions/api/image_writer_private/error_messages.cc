// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"

namespace extensions {
namespace image_writer {
namespace error {

// General operation messages.
const char kAborted[] = "ABORTED";
const char kNoOperationInProgress[] = "NO_OPERATION_IN_PROGRESS";
const char kOperationAlreadyInProgress[] = "OPERATION_ALREADY_IN_PROGRESS";
const char kUnsupportedOperation[] = "UNSUPPORTED_OPERATION";

// Device listing errors
const char kDeviceListError[] = "DEVICE_LIST_ERROR";

// File errors
const char kInvalidFileEntry[] = "FILE_INVALID";

// Download errors
const char kDownloadCancelled[] = "DOWNLOAD_CANCELLED";
const char kDownloadHashError[] = "DOWNLOAD_HASH_MISMATCH";
const char kDownloadInterrupted[] = "DOWNLOAD_INTERRUPTED";
const char kTempDirError[] = "TEMP_DIR_CREATION_ERROR";
const char kTempFileError[] = "TEMP_FILE_CREATION_ERROR";
const char kUrlInvalid[] = "URL_INVALID";

// Unzip errors
const char kUnzipGenericError[] = "UNZIP_ERROR";
const char kUnzipInvalidArchive[] = "UNZIP_INVALID_ARCHIVE";

// Write errors
const char kDeviceCloseError[] = "DEVICE_CLOSE_FAILED";
const char kDeviceInvalid[] = "DEVICE_INVALID";
const char kDeviceHashError[] = "DEVICE_HASH_ERROR";
const char kDeviceOpenError[] = "DEVICE_OPEN_ERROR";
const char kDeviceWriteError[] = "DEVICE_WRITE_ERROR";
const char kImageCloseError[] = "IMAGE_CLOSE_FAILED";
const char kImageInvalid[] = "IMAGE_INVALID";
const char kImageHashError[] = "IMAGE_HASH_ERROR";
const char kImageNotFound[] = "IMAGE_NOT_FOUND";
const char kImageOpenError[] = "IMAGE_OPEN_ERROR";
const char kImageReadError[] = "IMAGE_READ_ERROR";
const char kImageSizeError[] = "IMAGE_STAT_ERROR";
const char kUnmountVolumesError[] = "UNMOUNT_VOLUMES_ERROR";

// Verification Errors
const char kHashReadError[] = "HASH_READ_ERROR";
const char kVerificationFailed[] = "VERIFICATION_FAILED";

// Image burner catchall
const char kChromeOSImageBurnerError[] = "CHROMEOS_IMAGE_BURNER_ERROR";

} // namespace error
} // namespace image_writer
} // namespace extensions
