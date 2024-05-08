// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/crash_logging.h"

#include <ostream>
#include <string_view>

#include "build/build_config.h"

namespace base::debug {

namespace {

CrashKeyImplementation* g_crash_key_impl = nullptr;

}  // namespace

CrashKeyString* AllocateCrashKeyString(const char name[],
                                       CrashKeySize value_length) {
  if (!g_crash_key_impl)
    return nullptr;

    // TODO(crbug.com/40850825): It would be great if the DCHECKs below
    // could also be enabled on Android, but debugging tryjob failures was a bit
    // difficult... :-/
#if DCHECK_IS_ON() && !BUILDFLAG(IS_ANDROID)
  std::string_view name_piece = name;

  // Some `CrashKeyImplementation`s reserve certain characters and disallow
  // using them in crash key names.  See also https://crbug.com/1341077.
  DCHECK_EQ(std::string_view::npos, name_piece.find(':'))
      << "; name_piece = " << name_piece;

  // Some `CrashKeyImplementation`s support only short crash key names (e.g. see
  // the DCHECK in crash_reporter::internal::CrashKeyStringImpl::Set).
  // Enforcing this restrictions here ensures that crash keys will work for all
  // `CrashKeyStringImpl`s.
  DCHECK_LT(name_piece.size(), 40u);
#endif

  return g_crash_key_impl->Allocate(name, value_length);
}

void SetCrashKeyString(CrashKeyString* crash_key, std::string_view value) {
  if (!g_crash_key_impl || !crash_key)
    return;

  g_crash_key_impl->Set(crash_key, value);
}

void ClearCrashKeyString(CrashKeyString* crash_key) {
  if (!g_crash_key_impl || !crash_key)
    return;

  g_crash_key_impl->Clear(crash_key);
}

void OutputCrashKeysToStream(std::ostream& out) {
  if (!g_crash_key_impl)
    return;

  g_crash_key_impl->OutputCrashKeysToStream(out);
}

ScopedCrashKeyString::ScopedCrashKeyString(CrashKeyString* crash_key,
                                           std::string_view value)
    : crash_key_(crash_key) {
  SetCrashKeyString(crash_key_, value);
}

ScopedCrashKeyString::ScopedCrashKeyString(ScopedCrashKeyString&& other)
    : crash_key_(std::exchange(other.crash_key_, nullptr)) {}

ScopedCrashKeyString::~ScopedCrashKeyString() {
  ClearCrashKeyString(crash_key_);
}

void SetCrashKeyImplementation(std::unique_ptr<CrashKeyImplementation> impl) {
  delete g_crash_key_impl;
  g_crash_key_impl = impl.release();
}

}  // namespace base::debug
