// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_ARC_NEARBY_SHARE_UMA_H_
#define CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_ARC_NEARBY_SHARE_UMA_H_

#include "base/files/file.h"
#include "base/time/time.h"

namespace arc {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class ArcBridgeFailResult {
  kInstanceIsNull = 0,
  kAlreadyExists = 1,
  kMaxValue = kAlreadyExists,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DataHandlingResult {
  kDirectoryDoesNotExist = 0,
  kNullGBrowserProcess = 1,
  kInvalidProfile = 2,
  kEmptyDirectory = 3,
  kFailedToCreateDirectory = 4,
  kEmptyExternalURL = 5,
  kInvalidFileNameSize = 6,
  kInvalidFileSystemURL = 7,
  kNotExternalFileType = 8,
  kInvalidDestinationFileDescriptor = 9,
  kInvalidDestinationFilePath = 10,
  kFailedPrepTempDirectory = 11,
  kFailedStreamFileIOData = 12,
  kInvalidNumberBytesRead = 13,
  kTimeout = 14,
  kMaxValue = kTimeout,
};

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class IOErrorResult {
  kFileReaderFailed = 0,
  kDataPipeFailedWait = 1,
  kDataPipeUnexpectedClose = 2,
  kDataPipeFailedWrite = 3,
  kEOFWithRemainingBytes = 4,
  kReadFailed = 5,
  kStreamedDataInvalidEndpoint = 6,
  kMaxValue = kStreamedDataInvalidEndpoint,
};

void UpdateNearbyShareArcBridgeFail(ArcBridgeFailResult result);

void UpdateNearbyShareDataHandlingFail(DataHandlingResult result);

void UpdateNearbyShareIOFail(IOErrorResult result);

void UpdateNearbyShareWindowFound(bool found);

void UpdateNearbyShareFileStreamError(base::File::Error result);

void UpdateNearbyShareFileStreamCompleteTime(
    const base::TimeDelta& elapsed_time);
}  // namespace arc

#endif  // CHROME_BROWSER_ASH_ARC_NEARBY_SHARE_ARC_NEARBY_SHARE_UMA_H_
