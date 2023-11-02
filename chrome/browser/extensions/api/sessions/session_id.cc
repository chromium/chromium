// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/sessions/session_id.h"

#include <stddef.h>

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"

namespace extensions {

const char kIdSeparator = '.';

// static
std::unique_ptr<SessionId> SessionId::Parse(const std::string& session_id) {
  std::string session_tag;

  // Populate session_tag if the |session_id| represents a foreign SessionId.
  std::size_t separator = session_id.find(kIdSeparator);
  if (separator != std::string::npos) {
    session_tag = session_id.substr(0, separator);
  }

  // session_tag will be the empty string for local sessions that have only
  // a unique integer as the identifier.
  int id;
  if (!base::StringToInt(
      session_tag.empty() ? session_id : session_id.substr(separator + 1),
      &id)) {
    return nullptr;
  }
  return base::WrapUnique(new SessionId(session_tag, id));
}

SessionId::SessionId(const std::string& session_tag, int id)
    : session_tag_(session_tag), id_(id) {
}

bool SessionId::IsForeign() const {
  return !session_tag_.empty();
}

std::string SessionId::ToString() const {
  return IsForeign() ? (session_tag_ + kIdSeparator + base::NumberToString(id_))
                     : base::NumberToString(id_);
}

}  // namespace extensions
