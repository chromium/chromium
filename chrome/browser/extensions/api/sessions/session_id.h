// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSION_ID_H__
#define CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSION_ID_H__

#include <memory>
#include <string>

namespace extensions {

class SessionId {
 public:
  // Returns a SessionId, representing either a local or a foreign session.
  // In the case that the session is local, |session_tag_| will be empty string.
  // |session_string| should be in the format that ToString() would produce.
  static std::unique_ptr<SessionId> Parse(const std::string& session_string);

  // Constructs a SessionId object for the given session information.
  // |session_tag| is the string used to uniquely identify a synced foreign
  // session from the SessionModelAssociator. In the case that SessionId
  // represents a local session, |session_tag_| will be the empty string. |id|
  // uniquely identifies either a window or tab object in the local or the
  // |session_tag| session.
  SessionId(const std::string& session_tag, int id);

  SessionId(const SessionId&) = delete;
  SessionId& operator=(const SessionId&) = delete;

  // Returns true if the SessionId represents a foreign session.
  bool IsForeign() const;

  // Returns the compressed std::string representation of a SessionId in the
  // same format that Parse() accepts as its |session_string| parameter.
  std::string ToString() const;

  const std::string& session_tag() const { return session_tag_; }
  int id() const { return id_; }

 private:
  // The unique identifier for a foreign session, given by the
  // SessionModelAssociator.
  std::string session_tag_;

  // ID corresponding to a window or tab object.
  int id_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SESSIONS_SESSION_ID_H__
