// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/trace_event/trace_logging_minimal_win.h"

#include <evntrace.h>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"

TlmProvider::TlmProvider() noexcept = default;

TlmProvider::~TlmProvider() {
  Unregister();
}

TlmProvider::TlmProvider(const char* provider_name,
                         const GUID& provider_guid,
                         base::RepeatingCallback<void(EventControlCode)>
                             on_updated_callback) noexcept {
  ULONG status =
      Register(provider_name, provider_guid, std::move(on_updated_callback));
  LOG_IF(ERROR, status != ERROR_SUCCESS) << "Provider resistration failure";
}

// Appends a nul-terminated string to a metadata block.
// Returns new meta_data_index value, or -1 for overflow.
uint16_t TlmProvider::AppendNameToMetadata(
    char* metadata,
    uint16_t metadata_size,
    uint16_t metadata_index,
    std::string_view name) const noexcept {
  uint16_t index = metadata_index;
  DCHECK_LE(index, metadata_size);

  const size_t cch = name.size();
  if (cch + 1 > static_cast<unsigned>(metadata_size - index)) {
    return static_cast<uint16_t>(-1);
  }

  memcpy(metadata + index, name.data(), cch);
  metadata[index + cch] = 0;
  index += static_cast<uint16_t>(cch) + 1;
  return index;
}

void TlmProvider::Unregister() noexcept {
  if (reg_handle_ == 0)
    return;

  ULONG status = EventUnregister(reg_handle_);
  LOG_IF(ERROR, status != ERROR_SUCCESS) << "Provider unregistration failure";
  reg_handle_ = 0;
  level_plus1_ = 0;
}

ULONG TlmProvider::Register(const char* provider_name,
                            const GUID& provider_guid,
                            base::RepeatingCallback<void(EventControlCode)>
                                on_updated_callback) noexcept {
  // Calling Register when already registered is a fatal error.
  CHECK_EQ(reg_handle_, 0ULL);

  // provider_metadata_ for tracelogging has the following format:
  //     UINT16 metadata_size;
  //     char NullTerminatedUtf8ProviderName[];
  //     ( + optional extension data, not used here)

  // Append the provider name starting at offset 2 (skip MetadataSize).
  provider_metadata_size_ = AppendNameToMetadata(
      provider_metadata_, kMaxProviderMetadataSize, 2, provider_name);
  if (provider_metadata_size_ > kMaxProviderMetadataSize)
    return ERROR_BUFFER_OVERFLOW;

  // Fill in MetadataSize field at offset 0.
  *reinterpret_cast<uint16_t*>(provider_metadata_) = provider_metadata_size_;

  on_updated_callback_ = std::move(on_updated_callback);
  ULONG status =
      EventRegister(&provider_guid, StaticEnableCallback, this, &reg_handle_);
  if (status != ERROR_SUCCESS)
    return status;

  // Best-effort, ignore failure.
  return ::EventSetInformation(reg_handle_, EventProviderSetTraits,
                               provider_metadata_, provider_metadata_size_);
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

  TlmProvider* provider = static_cast<TlmProvider*>(callback_context);
  switch (is_enabled) {
    case EVENT_CONTROL_CODE_DISABLE_PROVIDER:
      provider->level_plus1_ = 0;
      break;
    case EVENT_CONTROL_CODE_ENABLE_PROVIDER:
      provider->level_plus1_ =
          level != 0 ? static_cast<unsigned>(level) + 1u : 256u;
      break;
  }
  provider->keyword_any_ = match_any_keyword;
  provider->keyword_all_ = match_all_keyword;

  if (provider->on_updated_callback_ &&
      is_enabled <= static_cast<size_t>(EventControlCode::kHighest)) {
    provider->on_updated_callback_.Run(
        static_cast<EventControlCode>(is_enabled));
  }
}

uint16_t TlmProvider::EventBegin(char* metadata,
                                 std::string_view event_name) const noexcept {
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
                                std::string_view field_name) const noexcept {
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
      *metadata_index = static_cast<uint16_t>(-1);
      return 0;
    }

    metadata[*metadata_index] = static_cast<char>(in_type);
    *metadata_index += 1;
    return 0;
  }
  // 2-byte encoding: in_type + out_type.
  if (kMaxEventMetadataSize - *metadata_index < 2) {
    *metadata_index = static_cast<uint16_t>(-1);
    return 0;
  }

  // Set high bit to indicate presence of OutType.
  metadata[*metadata_index] = static_cast<char>(in_type | 0x80);
  *metadata_index += 1;
  metadata[*metadata_index] = static_cast<char>(out_type);
  *metadata_index += 1;
  return 0;
}

ULONG TlmProvider::EventEnd(
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

TlmInt64Field::TlmInt64Field(const char* name, const int64_t value) noexcept
    : TlmFieldWithConstants(name), value_(value) {
  DCHECK_NE(Name().data(), nullptr);
}
int64_t TlmInt64Field::Value() const noexcept {
  return value_;
}
void TlmInt64Field::FillEventDescriptor(
    EVENT_DATA_DESCRIPTOR* descriptors) const noexcept {
  EventDataDescCreate(&descriptors[0], (void*)&value_, sizeof(value_));
}

TlmUInt64Field::TlmUInt64Field(const char* name, const uint64_t value) noexcept
    : TlmFieldWithConstants(name), value_(value) {
  DCHECK_NE(Name().data(), nullptr);
}
uint64_t TlmUInt64Field::Value() const noexcept {
  return value_;
}
void TlmUInt64Field::FillEventDescriptor(
    EVENT_DATA_DESCRIPTOR* descriptors) const noexcept {
  EventDataDescCreate(&descriptors[0], (void*)&value_, sizeof(value_));
}

TlmMbcsStringField::TlmMbcsStringField(const char* name,
                                       const char* value) noexcept
    : TlmFieldWithConstants(name), value_(value) {
  DCHECK_NE(Name().data(), nullptr);
  DCHECK_NE(value_, nullptr);
}

const char* TlmMbcsStringField::Value() const noexcept {
  return value_;
}

void TlmMbcsStringField::FillEventDescriptor(
    EVENT_DATA_DESCRIPTOR* descriptors) const noexcept {
  EventDataDescCreate(&descriptors[0], value_,
                      base::checked_cast<ULONG>(strlen(value_) + 1));
}

TlmUtf8StringField::TlmUtf8StringField(const char* name,
                                       const char* value) noexcept
    : TlmFieldWithConstants(name), value_(value) {
  DCHECK_NE(Name().data(), nullptr);
  DCHECK_NE(value_, nullptr);
}

const char* TlmUtf8StringField::Value() const noexcept {
  return value_;
}

void TlmUtf8StringField::FillEventDescriptor(
    EVENT_DATA_DESCRIPTOR* descriptors) const noexcept {
  EventDataDescCreate(&descriptors[0], value_,
                      base::checked_cast<ULONG>(strlen(value_) + 1));
}

TlmFieldBase::TlmFieldBase(const char* name) noexcept : name_(name) {}
TlmFieldBase::TlmFieldBase(std::string_view name) noexcept : name_(name) {}

TlmFieldBase::~TlmFieldBase() = default;

TlmFieldBase::TlmFieldBase(TlmFieldBase&&) noexcept = default;
TlmFieldBase& TlmFieldBase::operator=(TlmFieldBase&&) noexcept = default;
