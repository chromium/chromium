// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has several utility functions to open a media transfer protocol
// (MTP) device for communication, to enumerate the device contents, to read the
// device file object, etc. All these tasks may take an arbitary long time
// to complete. This file segregates those functionalities and runs them
// in the blocking pool thread rather than in the UI thread.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OPERATIONS_UTIL_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OPERATIONS_UTIL_H_

#include <portabledeviceapi.h>
#include <stddef.h>
#include <wrl/client.h>

#include <string>

#include "base/files/file.h"
#include "chrome/browser/media_galleries/win/mtp_device_object_entry.h"

namespace media_transfer_protocol {

// Opens the device for communication. |pnp_device_id| specifies the plug and
// play device ID string. On success, returns the IPortableDevice interface.
// On failure, returns NULL.
Microsoft::WRL::ComPtr<IPortableDevice> OpenDevice(
    const std::wstring& pnp_device_id);

// Gets the details of the object specified by |object_id| from the given MTP
// |device|. On success, returns no error (base::File::FILE_OK) and fills in
// |file_entry_info|. On failure, returns the corresponding platform file error
// and |file_entry_info| is not set.
base::File::Error GetFileEntryInfo(IPortableDevice* device,
                                   const std::wstring& object_id,
                                   base::File::Info* file_entry_info);

// Gets the entries of the directory specified by |directory_object_id| from
// the given MTP |device|. On success, returns true and fills in
// |object_entries|. On failure, returns false and |object_entries| is not
// set.
bool GetDirectoryEntries(IPortableDevice* device,
                         const std::wstring& directory_object_id,
                         MTPDeviceObjectEntries* object_entries);

// Gets an IStream interface to read the object content data from the |device|.
// |file_object_id| specifies the device file object identifier.
// On success, returns S_OK and sets |file_stream| and |optimal_transfer_size|.
// On failure, returns an error code and |file_stream| and
// |optimal_transfer_size| are not set.
HRESULT GetFileStreamForObject(IPortableDevice* device,
                               const std::wstring& file_object_id,
                               IStream** file_stream,
                               DWORD* optimal_transfer_size);

// Copies a data chunk from |stream| to the file specified by the |local_path|.
// |optimal_transfer_size| specifies the optimal data transfer size.
//
// On success, appends the data chunk in |local_path| and returns a non-zero
// value indicating the total number of bytes written to the file specified
// by the |local_path|. If the end of the |stream| is not reached,
// the return value will be equal to |optimal_transfer_size|. If the end of the
// |stream| is reached, the return value will be less than or equal to
// |optimal_transfer_size|.
//
// On failure, returns 0.
DWORD CopyDataChunkToLocalFile(IStream* stream,
                               const base::FilePath& local_path,
                               size_t optimal_transfer_size);

// Returns the identifier of the object specified by the |object_name|.
// |parent_id| specifies the object's parent identifier.
// |object_name| specifies the friendly name of the object.
std::wstring GetObjectIdFromName(IPortableDevice* device,
                                 const std::wstring& parent_id,
                                 const std::u16string& object_name);

}  // namespace media_transfer_protocol

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_WIN_MTP_DEVICE_OPERATIONS_UTIL_H_
