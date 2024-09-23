// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/win/mtp_device_operations_util.h"

#include <objbase.h>

#include <portabledevice.h>
#include <stdint.h>

#include <algorithm>
#include <limits>
#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/scoped_propvariant.h"
#include "chrome/common/chrome_constants.h"

namespace media_transfer_protocol {

namespace {

// On success, returns true and updates |client_info| with a reference to an
// IPortableDeviceValues interface that holds information about the
// application that communicates with the device.
bool GetClientInformation(
    Microsoft::WRL::ComPtr<IPortableDeviceValues>* client_info) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(client_info);
  HRESULT hr =
      ::CoCreateInstance(__uuidof(PortableDeviceValues), NULL,
                         CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&(*client_info)));
  if (FAILED(hr)) {
    DPLOG(ERROR) << "Failed to create an instance of IPortableDeviceValues";
    return false;
  }

  (*client_info)->SetStringValue(WPD_CLIENT_NAME,
                                 chrome::kBrowserProcessExecutableName);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_MAJOR_VERSION, 0);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_MINOR_VERSION, 0);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_REVISION, 0);
  (*client_info)->SetUnsignedIntegerValue(
      WPD_CLIENT_SECURITY_QUALITY_OF_SERVICE, SECURITY_IMPERSONATION);
  (*client_info)->SetUnsignedIntegerValue(WPD_CLIENT_DESIRED_ACCESS,
                                          GENERIC_READ);
  return true;
}

// Gets the content interface of the portable |device|. On success, returns
// the IPortableDeviceContent interface. On failure, returns NULL.
Microsoft::WRL::ComPtr<IPortableDeviceContent> GetDeviceContent(
    IPortableDevice* device) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(device);
  Microsoft::WRL::ComPtr<IPortableDeviceContent> content;
  if (SUCCEEDED(device->Content(&content)))
    return content;
  return Microsoft::WRL::ComPtr<IPortableDeviceContent>();
}

// On success, returns IEnumPortableDeviceObjectIDs interface to enumerate
// the device objects. On failure, returns NULL.
// |parent_id| specifies the parent object identifier.
Microsoft::WRL::ComPtr<IEnumPortableDeviceObjectIDs> GetDeviceObjectEnumerator(
    IPortableDevice* device,
    const std::wstring& parent_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(device);
  DCHECK(!parent_id.empty());
  Microsoft::WRL::ComPtr<IPortableDeviceContent> content =
      GetDeviceContent(device);
  if (!content.Get())
    return Microsoft::WRL::ComPtr<IEnumPortableDeviceObjectIDs>();

  Microsoft::WRL::ComPtr<IEnumPortableDeviceObjectIDs> enum_object_ids;
  if (SUCCEEDED(
          content->EnumObjects(0, parent_id.c_str(), NULL, &enum_object_ids)))
    return enum_object_ids;
  return Microsoft::WRL::ComPtr<IEnumPortableDeviceObjectIDs>();
}

// Returns whether the object is a directory/folder/album. |properties_values|
// contains the object property key values.
bool IsDirectory(IPortableDeviceValues* properties_values) {
  DCHECK(properties_values);
  GUID content_type;
  HRESULT hr = properties_values->GetGuidValue(WPD_OBJECT_CONTENT_TYPE,
                                               &content_type);
  if (FAILED(hr))
    return false;
  // TODO(kmadhusu): |content_type| can be an image or audio or video or mixed
  // album. It is not clear whether an album is a collection of physical objects
  // or virtual objects. Investigate this in detail.

  // The root storage object describes its content type as
  // WPD_CONTENT_FUNCTIONAL_OBJECT.
  return (content_type == WPD_CONTENT_TYPE_FOLDER ||
          content_type == WPD_CONTENT_TYPE_FUNCTIONAL_OBJECT);
}

// Returns the name of the object from |properties_values|. If the object has
// no filename, try to use a friendly name instead. e.g. with MTP storage roots.
std::u16string GetObjectName(IPortableDeviceValues* properties_values) {
  DCHECK(properties_values);
  base::win::ScopedCoMem<wchar_t> buffer;
  HRESULT hr = properties_values->GetStringValue(WPD_OBJECT_ORIGINAL_FILE_NAME,
                                                 &buffer);
  if (FAILED(hr))
    hr = properties_values->GetStringValue(WPD_OBJECT_NAME, &buffer);
  if (FAILED(hr))
    return std::u16string();
  return base::WideToUTF16(std::wstring(buffer));
}

// Gets the last modified time of the object from the property key values
// specified by the |properties_values|. On success, fills in
// |last_modified_time|.
void GetLastModifiedTime(IPortableDeviceValues* properties_values,
                         base::Time* last_modified_time) {
  DCHECK(properties_values);
  DCHECK(last_modified_time);
  base::win::ScopedPropVariant last_modified_date;
  HRESULT hr = properties_values->GetValue(WPD_OBJECT_DATE_MODIFIED,
                                           last_modified_date.Receive());
  if (FAILED(hr))
    return;

  // Some PTP devices don't provide an mtime. Try using the ctime instead.
  if (last_modified_date.get().vt != VT_DATE) {
    last_modified_date.Reset();
    hr = properties_values->GetValue(WPD_OBJECT_DATE_CREATED,
                                     last_modified_date.Receive());
    if (FAILED(hr))
      return;
  }

  SYSTEMTIME system_time;
  FILETIME file_time;
  if (last_modified_date.get().vt == VT_DATE &&
      VariantTimeToSystemTime(last_modified_date.get().date, &system_time) &&
      SystemTimeToFileTime(&system_time, &file_time)) {
    *last_modified_time = base::Time::FromFileTime(file_time);
  }
}

// Gets the size of the file object in bytes from the property key values
// specified by the |properties_values|. On failure, return -1.
int64_t GetObjectSize(IPortableDeviceValues* properties_values) {
  DCHECK(properties_values);
  ULONGLONG actual_size;
  HRESULT hr = properties_values->GetUnsignedLargeIntegerValue(WPD_OBJECT_SIZE,
                                                               &actual_size);
  bool success = SUCCEEDED(hr) &&
                 (actual_size <=
                  static_cast<ULONGLONG>(std::numeric_limits<int64_t>::max()));
  return success ? static_cast<int64_t>(actual_size) : -1;
}

// Gets the details of the object specified by the |object_id| given the media
// transfer protocol |device|. On success, returns true and fills in |name|,
// |is_directory|, |size|. |last_modified_time| will be filled in if possible,
// but failure to get it doesn't prevent success.
bool GetObjectDetails(IPortableDevice* device,
                      const std::wstring object_id,
                      std::u16string* name,
                      bool* is_directory,
                      int64_t* size,
                      base::Time* last_modified_time) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(device);
  DCHECK(!object_id.empty());
  DCHECK(name);
  DCHECK(is_directory);
  DCHECK(size);
  DCHECK(last_modified_time);
  Microsoft::WRL::ComPtr<IPortableDeviceContent> content =
      GetDeviceContent(device);
  if (!content.Get())
    return false;

  Microsoft::WRL::ComPtr<IPortableDeviceProperties> properties;
  HRESULT hr = content->Properties(&properties);
  if (FAILED(hr))
    return false;

  Microsoft::WRL::ComPtr<IPortableDeviceKeyCollection> properties_to_read;
  hr = ::CoCreateInstance(__uuidof(PortableDeviceKeyCollection), NULL,
                          CLSCTX_INPROC_SERVER,
                          IID_PPV_ARGS(&properties_to_read));
  if (FAILED(hr))
    return false;

  if (FAILED(properties_to_read->Add(WPD_OBJECT_CONTENT_TYPE)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_FORMAT)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_ORIGINAL_FILE_NAME)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_NAME)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_DATE_MODIFIED)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_DATE_CREATED)) ||
      FAILED(properties_to_read->Add(WPD_OBJECT_SIZE)))
    return false;

  Microsoft::WRL::ComPtr<IPortableDeviceValues> properties_values;
  hr = properties->GetValues(object_id.c_str(), properties_to_read.Get(),
                             &properties_values);
  if (FAILED(hr))
    return false;

  *is_directory = IsDirectory(properties_values.Get());
  *name = GetObjectName(properties_values.Get());
  if (name->empty())
    return false;

  if (*is_directory) {
    // Directory entry does not have size and last modified date property key
    // values.
    *size = 0;
    *last_modified_time = base::Time();
    return true;
  }

  // Try to get the last modified time, but don't fail if we can't.
  GetLastModifiedTime(properties_values.Get(), last_modified_time);

  int64_t object_size = GetObjectSize(properties_values.Get());
  if (object_size < 0)
    return false;
  *size = object_size;
  return true;
}

// Creates an MTP device object entry for the given |device| and |object_id|.
// On success, returns true and fills in |entry|.
MTPDeviceObjectEntry GetMTPDeviceObjectEntry(IPortableDevice* device,
                                             const std::wstring& object_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(device);
  DCHECK(!object_id.empty());
  std::u16string name;
  bool is_directory;
  int64_t size;
  base::Time last_modified_time;
  MTPDeviceObjectEntry entry;
  if (GetObjectDetails(device, object_id, &name, &is_directory, &size,
                       &last_modified_time)) {
    entry = MTPDeviceObjectEntry(object_id, name, is_directory, size,
                                 last_modified_time);
  }
  return entry;
}

// Gets the entries of the directory specified by |directory_object_id| from
// the given MTP |device|. To request a specific object entry, put the object
// name in |object_name|. Leave |object_name| blank to request all entries. On
// success returns true and set |object_entries|.
bool GetMTPDeviceObjectEntries(IPortableDevice* device,
                               const std::wstring& directory_object_id,
                               const std::u16string& object_name,
                               MTPDeviceObjectEntries* object_entries) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(device);
  DCHECK(!directory_object_id.empty());
  DCHECK(object_entries);
  Microsoft::WRL::ComPtr<IEnumPortableDeviceObjectIDs> enum_object_ids =
      GetDeviceObjectEnumerator(device, directory_object_id);
  if (!enum_object_ids.Get())
    return false;

  // Loop calling Next() while S_OK is being returned.
  const DWORD num_objects_to_request = 10;
  const bool get_all_entries = object_name.empty();
  for (HRESULT hr = S_OK; hr == S_OK;) {
    DWORD num_objects_fetched = 0;
    std::unique_ptr<wchar_t*[]> object_ids(
        new wchar_t*[num_objects_to_request]);
    hr = enum_object_ids->Next(num_objects_to_request,
                               object_ids.get(),
                               &num_objects_fetched);
    for (DWORD i = 0; i < num_objects_fetched; ++i) {
      MTPDeviceObjectEntry entry =
          GetMTPDeviceObjectEntry(device, object_ids[i]);
      if (entry.object_id.empty())
        continue;
      if (get_all_entries) {
        object_entries->push_back(entry);
      } else if (entry.name == object_name) {
        object_entries->push_back(entry);  // Object entry found.
        break;
      }
    }
    for (DWORD i = 0; i < num_objects_fetched; ++i)
      CoTaskMemFree(object_ids[i]);
  }
  return true;
}

}  // namespace

Microsoft::WRL::ComPtr<IPortableDevice> OpenDevice(
    const std::wstring& pnp_device_id) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(!pnp_device_id.empty());
  Microsoft::WRL::ComPtr<IPortableDeviceValues> client_info;
  if (!GetClientInformation(&client_info))
    return Microsoft::WRL::ComPtr<IPortableDevice>();
  Microsoft::WRL::ComPtr<IPortableDevice> device;
  HRESULT hr = ::CoCreateInstance(__uuidof(PortableDevice), NULL,
                                  CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&device));
  if (FAILED(hr))
    return Microsoft::WRL::ComPtr<IPortableDevice>();

  hr = device->Open(pnp_device_id.c_str(), client_info.Get());
  if (SUCCEEDED(hr))
    return device;
  if (hr == E_ACCESSDENIED)
    DPLOG(ERROR) << "Access denied to open the device";
  return Microsoft::WRL::ComPtr<IPortableDevice>();
}

base::File::Error GetFileEntryInfo(IPortableDevice* device,
                                   const std::wstring& object_id,
                                   base::File::Info* file_entry_info) {
  DCHECK(device);
  DCHECK(!object_id.empty());
  DCHECK(file_entry_info);
  MTPDeviceObjectEntry entry = GetMTPDeviceObjectEntry(device, object_id);
  if (entry.object_id.empty())
    return base::File::FILE_ERROR_NOT_FOUND;

  file_entry_info->size = entry.size;
  file_entry_info->is_directory = entry.is_directory;
  file_entry_info->is_symbolic_link = false;
  file_entry_info->last_modified = entry.last_modified_time;
  file_entry_info->last_accessed = entry.last_modified_time;
  file_entry_info->creation_time = base::Time();
  return base::File::FILE_OK;
}

bool GetDirectoryEntries(IPortableDevice* device,
                         const std::wstring& directory_object_id,
                         MTPDeviceObjectEntries* object_entries) {
  return GetMTPDeviceObjectEntries(device, directory_object_id,
                                   std::u16string(), object_entries);
}

HRESULT GetFileStreamForObject(IPortableDevice* device,
                               const std::wstring& file_object_id,
                               IStream** file_stream,
                               DWORD* optimal_transfer_size) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(device);
  DCHECK(!file_object_id.empty());
  Microsoft::WRL::ComPtr<IPortableDeviceContent> content =
      GetDeviceContent(device);
  if (!content.Get())
    return E_FAIL;

  Microsoft::WRL::ComPtr<IPortableDeviceResources> resources;
  HRESULT hr = content->Transfer(&resources);
  if (FAILED(hr))
    return hr;
  return resources->GetStream(file_object_id.c_str(), WPD_RESOURCE_DEFAULT,
                              STGM_READ, optimal_transfer_size,
                              file_stream);
}

DWORD CopyDataChunkToLocalFile(IStream* stream,
                               const base::FilePath& local_path,
                               size_t optimal_transfer_size) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  DCHECK(stream);
  DCHECK(!local_path.empty());
  if (optimal_transfer_size == 0U)
    return 0U;
  DWORD bytes_read = 0;
  std::string buffer;
  HRESULT hr = stream->Read(base::WriteInto(&buffer, optimal_transfer_size + 1),
                            optimal_transfer_size, &bytes_read);
  // IStream::Read() returns S_FALSE when the actual number of bytes read from
  // the stream object is less than the number of bytes requested (aka
  // |optimal_transfer_size|). This indicates the end of the stream has been
  // reached.
  if (FAILED(hr))
    return 0U;
  DCHECK_GT(bytes_read, 0U);
  CHECK_LE(bytes_read, buffer.size());
  buffer.resize(bytes_read);
  return base::AppendToFile(local_path, buffer) ? buffer.size() : 0;
}

std::wstring GetObjectIdFromName(IPortableDevice* device,
                                 const std::wstring& parent_id,
                                 const std::u16string& object_name) {
  MTPDeviceObjectEntries object_entries;
  if (!GetMTPDeviceObjectEntries(device, parent_id, object_name,
                                 &object_entries) ||
      object_entries.empty())
    return std::wstring();
  // TODO(thestig): This DCHECK can fail. Multiple MTP objects can have
  // the same name. Handle the situation gracefully. Refer to crbug.com/169930
  // for more details.
  DCHECK_EQ(1U, object_entries.size());
  return object_entries[0].object_id;
}

}  // namespace media_transfer_protocol
