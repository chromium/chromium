// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_logging_minimal_win.h"

#include <evntrace.h>

#include "base/check_op.h"

/*
EventSetInformation configuration macros:

TraceLogging works best if the EventSetInformation API can be used to notify
ETW that the provider uses TraceLogging event encoding.

The EventSetInformation API is available on Windows 8 and later. (It is also
available on fully-patched Windows 7, but not on Windows 7 RTM).

The TLM_HAVE_EVENT_SET_INFORMATION and TLM_EVENT_SET_INFORMATION macros can
be set before compiling this file to  control how the TlmProvider class deals
with the EventSetInformation API.

If these macros are not set, the default behavior is to check the WINVER
macro at compile time:

- If WINVER is set to Windows 7 or before, TlmProvider will use GetProcAddress
  to locate EventSetInformation, and then invoke it if present. This is less
  efficient, but works on older versions of Windows.
- If WINVER is set to Windows 8 or later, TlmProvider will directly invoke
  EventSetInformation. This is more efficient, but the resulting application
  will only work correctly on newer versions of Windows.

If you need to run on Windows 7 RTM, but for some reason need to set WINVER to
Windows 8 or higher, you can override the default behavior by defining
TLM_HAVE_EVENT_SET_INFORMATION=2 when compiling this file.

Details:
- The TLM_EVENT_SET_INFORMATION macro can be set the name of a replacement
  function that TlmProvider should use instead of EventSetInformation.
- The TLM_HAVE_EVENT_SET_INFORMATION macro can be set to 0 (disable the use of
  EventSetInformation), 1 (directly invoke EventSetInformation), or 2 (try to
  locate EventSetInformation via GetProcAddress, and invoke if found).
*/

// This code needs to run on Windows 7 and this is magic which
// removes static linking to EventSetInformation
#define TLM_HAVE_EVENT_SET_INFORMATION 2

#ifndef TLM_EVENT_SET_INFORMATION
#define TLM_EVENT_SET_INFORMATION EventSetInformation
#ifndef TLM_HAVE_EVENT_SET_INFORMATION
#if WINVER < 0x0602 || !defined(EVENT_FILTER_TYPE_SCHEMATIZED)
// Find "EventSetInformation" via GetModuleHandleExW+GetProcAddress
#define TLM_HAVE_EVENT_SET_INFORMATION 2
#else
// Directly invoke TLM_EVENT_SET_INFORMATION(...)
#define TLM_HAVE_EVENT_SET_INFORMATION 1
#endif
#endif
#elif !defined(TLM_HAVE_EVENT_SET_INFORMATION)
// Directly invoke TLM_EVENT_SET_INFORMATION(...)
#define TLM_HAVE_EVENT_SET_INFORMATION 1
#endif

TlmProvider::~TlmProvider() {
  Unregister();
}

TlmProvider::TlmProvider(const char* provider_name,
                         const GUID& provider_guid,
                         PENABLECALLBACK enable_callback,
                         void* enable_callback_context) noexcept {
  int32_t status = Register(provider_name, provider_guid, enable_callback,
                            enable_callback_context);
  DCHECK_EQ(status, ERROR_SUCCESS);
}

// Appends a nul-terminated string to a metadata block.
// Returns new meta_data_index value, or -1 for overflow.
uint16_t TlmProvider::AppendNameToMetadata(char* metadata,
                                           uint16_t metadata_size,
                                           uint16_t metadata_index,
                                           const char* name) const noexcept {
  uint16_t index = metadata_index;
  DCHECK_LE(index, metadata_size);

  const size_t cch = strlen(name) + 1;
  if (cch > static_cast<unsigned>(metadata_size - index)) {
    index = -1;
  } else {
    memcpy(metadata + index, name, cch);
    index += static_cast<uint16_t>(cch);
  }

  return index;
}

void TlmProvider::Unregister() noexcept {
  if (reg_handle_ == 0)
    return;

  int32_t status = EventUnregister(reg_handle_);
  DCHECK_EQ(status, ERROR_SUCCESS);
  reg_handle_ = 0;
  level_plus1_ = 0;
}

int32_t TlmProvider::Register(const char* provider_name,
                              const GUID& provider_guid,
                              PENABLECALLBACK enable_callback,
                              void* enable_callback_context) noexcept {
  // Calling Register when already registered is a fatal error.
  CHECK_EQ(reg_handle_, 0ULL);

  // provider_metadata_ for tracelogging has the following format:
  //     UINT16 metadata_size;
  //     char NullTerminatedUtf8ProviderName[];
  //     ( + optional extension data, not used here)

  // Append the provider name starting at offset 2 (skip MetadataSize).
  provider_metadata_size_ = AppendNameToMetadata(
      provider_metadata_, kMaxProviderMetadataSize, 2, provider_name);
  if (provider_metadata_size_ > kMaxProviderMetadataSize) {
    DCHECK_GT(provider_metadata_size_, kMaxProviderMetadataSize);
    return ERROR_BUFFER_OVERFLOW;
  }

  // Fill in MetadataSize field at offset 0.
  *reinterpret_cast<uint16_t*>(provider_metadata_) = provider_metadata_size_;

  enable_callback_ = enable_callback;
  enable_callback_context_ = enable_callback_context;
  int32_t status =
      EventRegister(&provider_guid, StaticEnableCallback, this, &reg_handle_);
  if (status != ERROR_SUCCESS)
    return status;

#if TLM_HAVE_EVENT_SET_INFORMATION == 1

  // Best-effort, ignore failure.
  status =
      TLM_EVENT_SET_INFORMATION(reg_handle_, EventProviderSetTraits,
                                provider_metadata_, provider_metadata_size_);

#elif TLM_HAVE_EVENT_SET_INFORMATION == 2

  HMODULE eventing_lib;
  if (GetModuleHandleExW(0, L"api-ms-win-eventing-provider-l1-1-0.dll",
                         &eventing_lib) ||
      GetModuleHandleExW(0, L"advapi32.dll", &eventing_lib)) {
    typedef ULONG(WINAPI * PFEventSetInformation)(
        REGHANDLE reg_handle, EVENT_INFO_CLASS information_class,
        PVOID event_information, ULONG information_length);
    PFEventSetInformation event_set_information_ptr =
        reinterpret_cast<decltype(&::EventSetInformation)>(
            GetProcAddress(eventing_lib, "EventSetInformation"));
    if (event_set_information_ptr) {
      // Best-effort, ignore failure.
      status = event_set_information_ptr(reg_handle_, EventProviderSetTraits,
                                         provider_metadata_,
                                         provider_metadata_size_);
      DCHECK_EQ(status, ERROR_SUCCESS);
    }

    FreeLibrary(eventing_lib);
  }

#else  // TLM_HAVE_EVENT_SET_INFORMATION == 0

    // Make no attempt to invoke EventSetInformation.

#endif  // TLM_HAVE_EVENT_SET_INFORMATION

  return status;
}

bool TlmProvider::IsEnabled() const noexcept {
  return 0 < level_plus1_;
}

bool TlmProvider::IsEnabled(uint8_t level) const noexcept {
  return level < level_plus1_;
}

bool TlmProvider::IsEnabled(uint8_t level, uint64_t keyword) const noexcept {
  return level < level_plus1_ && KeywordEnabled(keyword);
}

bool TlmProvider::IsEnabled(
    const EVENT_DESCRIPTOR& event_descriptor) const noexcept {
  return event_descriptor.Level < level_plus1_ &&
         KeywordEnabled(event_descriptor.Keyword);
}

void TlmProvider::StaticEnableCallback(const GUID* source_id,
                                       ULONG is_enabled,
                                       UCHAR level,
                                       ULONGLONG match_any_keyword,
                                       ULONGLONG match_all_keyword,
                                       PEVENT_FILTER_DESCRIPTOR filter_data,
                                       PVOID callback_context) {
  if (!callback_context)
    return;

  TlmProvider* pProvider = static_cast<TlmProvider*>(callback_context);
  switch (is_enabled) {
    case EVENT_CONTROL_CODE_DISABLE_PROVIDER:
      pProvider->level_plus1_ = 0;
      break;
    case EVENT_CONTROL_CODE_ENABLE_PROVIDER:
      pProvider->level_plus1_ =
          level != 0 ? static_cast<unsigned>(level) + 1u : 256u;
      pProvider->keyword_any_ = match_any_keyword;
      pProvider->keyword_all_ = match_all_keyword;
      break;
  }

  if (pProvider->enable_callback_) {
    pProvider->enable_callback_(source_id, is_enabled, level, match_any_keyword,
                                match_all_keyword, filter_data,
                                pProvider->enable_callback_context_);
  }
}

uint16_t TlmProvider::EventBegin(char* metadata,
                                 const char* event_name) const noexcept {
  // EventMetadata for tracelogging has the following format
  //     UINT16 MetadataSize;
  //     BYTE SpecialFlags[]; // Not used, so always size 1.
  //     char NullTerminatedUtf8EventName[];
  //     ( + field definitions)

  uint16_t index = 2;  // Skip MetadataSize field.

  metadata[index] = 0;  // Set SpecialFlags[0] = 0.
  index++;              // sizeof(SpecialFlags) == 1.

  index =
      AppendNameToMetadata(metadata, kMaxEventMetadataSize, index, event_name);
  return index;
}

char TlmProvider::EventAddField(char* metadata,
                                uint16_t* metadata_index,
                                uint8_t in_type,
                                uint8_t out_type,
                                const char* field_name) const noexcept {
  DCHECK_LT(in_type, 0x80);
  DCHECK_LT(out_type, 0x80);

  // FieldDefinition =
  //     char NullTerminatedUtf8FieldName[];
  //     BYTE InType;
  //     BYTE OutType; // Only present if high bit set in InType.
  //     ( + optional extension data not used here)

  if (*metadata_index >= kMaxEventMetadataSize)
    return 0;

  *metadata_index = AppendNameToMetadata(metadata, kMaxEventMetadataSize,
                                         *metadata_index, field_name);
  if (*metadata_index >= kMaxEventMetadataSize)
    return 0;

  if (out_type == 0) {
    // 1-byte encoding: inType + TlgOutNULL.
    if (1 > kMaxEventMetadataSize - *metadata_index) {
      *metadata_index = -1;
      return 0;
    }

    metadata[*metadata_index] = in_type;
    *metadata_index += 1;
    return 0;
  }
  // 2-byte encoding: in_type + out_type.
  if (kMaxEventMetadataSize - *metadata_index < 2) {
    *metadata_index = -1;
    return 0;
  }

  // Set high bit to indicate presence of OutType.
  metadata[*metadata_index] = in_type | 0x80;
  *metadata_index += 1;
  metadata[*metadata_index] = out_type;
  *metadata_index += 1;
  return 0;
}

int32_t TlmProvider::EventEnd(
    char* metadata,
    uint16_t meta_data_index,
    EVENT_DATA_DESCRIPTOR* descriptors,
    uint32_t descriptors_index,
    const EVENT_DESCRIPTOR& event_descriptor) const noexcept {
  if (meta_data_index > kMaxEventMetadataSize) {
    return ERROR_BUFFER_OVERFLOW;
  }

  // Fill in EventMetadata's MetadataSize field.
  *reinterpret_cast<uint16_t*>(metadata) = meta_data_index;

  descriptors[0].Ptr = reinterpret_cast<ULONG_PTR>(provider_metadata_);
  descriptors[0].Size = provider_metadata_size_;
  descriptors[0].Reserved = EVENT_DATA_DESCRIPTOR_TYPE_PROVIDER_METADATA;

  descriptors[1].Ptr = reinterpret_cast<ULONG_PTR>(metadata);
  descriptors[1].Size = meta_data_index;
  descriptors[1].Reserved = EVENT_DATA_DESCRIPTOR_TYPE_EVENT_METADATA;

  return EventWrite(reg_handle_, &event_descriptor, descriptors_index,
                    descriptors);
}

bool TlmProvider::KeywordEnabled(uint64_t keyword) const noexcept {
  return keyword == 0 ||
         ((keyword & keyword_any_) && (keyword & keyword_all_) == keyword_all_);
}

TlmMbcsStringField::TlmMbcsStringField(const char* name,
                                       const char* value) noexcept
    : TlmFieldBase(name), value_(value) {
  DCHECK_NE(Name(), nullptr);
  DCHECK_NE(value_, nullptr);
}

const char* TlmMbcsStringField::Value() const noexcept {
  return value_;
}

void TlmMbcsStringField::FillEventDescriptor(
    EVENT_DATA_DESCRIPTOR* descriptors) const noexcept {
  EventDataDescCreate(&descriptors[0], value_, strlen(value_) + 1);
}

TlmUtf8StringField::TlmUtf8StringField(const char* name,
                                       const char* value) noexcept
    : TlmFieldBase(name), value_(value) {
  DCHECK_NE(Name(), nullptr);
  DCHECK_NE(value_, nullptr);
}

const char* TlmUtf8StringField::Value() const noexcept {
  return value_;
}

void TlmUtf8StringField::FillEventDescriptor(
    EVENT_DATA_DESCRIPTOR* descriptors) const noexcept {
  EventDataDescCreate(&descriptors[0], value_, strlen(value_) + 1);
}
