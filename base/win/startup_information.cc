// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/startup_information.h"

#include "base/logging.h"

namespace base {
namespace win {

StartupInformation::StartupInformation() : startup_info_() {
  startup_info_.StartupInfo.cb = sizeof(startup_info_);
}

StartupInformation::~StartupInformation() {
  if (startup_info_.lpAttributeList) {
    ::DeleteProcThreadAttributeList(startup_info_.lpAttributeList);
  }
}

bool StartupInformation::InitializeProcThreadAttributeList(
    DWORD attribute_count) {
  if (startup_info_.StartupInfo.cb != sizeof(startup_info_) ||
      startup_info_.lpAttributeList) {
    return false;
  }

  SIZE_T size = 0;
  ::InitializeProcThreadAttributeList(nullptr, attribute_count, 0, &size);
  if (size == 0)
    return false;

  auto attribute_list = std::make_unique<char[]>(size);
  auto* attribute_list_ptr =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attribute_list.get());
  if (!::InitializeProcThreadAttributeList(attribute_list_ptr, attribute_count,
                                           0, &size)) {
    return false;
  }

  attribute_list_ = std::move(attribute_list);
  startup_info_.lpAttributeList = attribute_list_ptr;

  return true;
}

bool StartupInformation::UpdateProcThreadAttribute(
    DWORD_PTR attribute,
    void* value,
    size_t size) {
  if (!startup_info_.lpAttributeList)
    return false;
  return !!::UpdateProcThreadAttribute(startup_info_.lpAttributeList, 0,
                                       attribute, value, size, nullptr,
                                       nullptr);
}

}  // namespace win
}  // namespace base

