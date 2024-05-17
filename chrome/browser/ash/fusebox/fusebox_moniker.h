// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_MONIKER_H_
#define CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_MONIKER_H_

#include <map>
#include <utility>

#include "base/token.h"
#include "base/values.h"
#include "storage/browser/file_system/file_system_url.h"

namespace fusebox {

// A moniker is an alternative name (an alias or symbolic link, of sorts) for a
// FileSystemURL (a C++ object). That name is a filename on the Linux file
// system, such as "/media/fuse/fusebox/moniker/1234etc", and is served by the
// FuseBox FUSE server.
//
// It is like a symbolic link, "ln -s TARGET LINK_NAME", where TARGET is the
// FileSystemURL and LINK_NAME is the "/media/fuse/fusebox/moniker/1234etc",
// but differs from symlinks in three ways.
//
// First, the TARGET is itself not present in the file system (at the operating
// system level). The purpose of a moniker is to provide a filename (in the OS
// sense) for something that doesn't have one (such as an Android Content
// Provider's "content://com.example.appname.provider/the/path/to/the/thing"
// Content URL wrapped in a FileSystemURL). Separate processes can share a
// filename (a plain old string) when they can't share a FileSystemURL.
//
// Second, the LINK_NAME is unguessable (essentially a randomly generated
// 128-bit base::Token) and "ls /media/fuse/fusebox/moniker/" will show an
// empty directory. Only the CreateMoniker caller (and whoever it shares the
// resultant Moniker with, in C++ object, base::Token or string filename form)
// has the capability to ask the FuseBox FUSE server to resolve the LINK_NAME
// to read the original TARGET.
//
// Third, monikers always target individual files, never directories.
//
// Its base::Token (an 128-bit value) is randomly generated (by CreateMoniker)
// and unguessable, so in some sense, it is like a base::UnguessableToken (call
// that a b::UT). But an all-zero-bits b::UT is not just invalid,
// b::UT::Deserialize(0, 0) will actually DCHECK-crash. The design assumption
// is that b::UT values are only shared between trusted processes via trusted
// channels. Here, the token is parsed from the FUSE filename and we don't want
// "ls /media/fuse/fusebox/moniker/00000000000000000000000000000000" to crash
// the Chrome process. So we use base::Token, a more forgiving type than
// base::UnguessableToken.
//
// See also the crrev.com/c/3645173 code review discussion.
using Moniker = base::Token;

// Maps from Moniker to storage::FileSystemURL target.
//
// All non-static methods must only be called on the main (UI) thread.
class MonikerMap {
 public:
  struct ExtractTokenResult {
    enum class ResultType {
      // The fs_url_as_string was a Moniker FileSystemURL (it started with
      // "moniker/") and held a well-formed token.
      OK = 0,
      // The fs_url_as_string was not a Moniker FileSystemURL.
      NOT_A_MONIKER_FS_URL = 1,
      // The fs_url_as_string was a Moniker FileSystemURL but named the root of
      // all such FileSystemURL's. It did not hold a well-formed token.
      MONIKER_FS_URL_BUT_ONLY_ROOT = 2,
      // The fs_url_as_string was a Moniker FileSystemURL but did not hold a
      // well-formed token (and was not MONIKER_FS_URL_BUT_ONLY_ROOT).
      MONIKER_FS_URL_BUT_NOT_WELL_FORMED = 3,
    } result_type;

    base::Token token;
  };

  using FSURLAndReadOnlyState = std::pair<storage::FileSystemURL, bool>;

  // Returns the 1234etc base::Token from a Fusebox relative path (like
  // "moniker/1234etc"), where "moniker" is the fusebox::kMonikerSubdir prefix.
  //
  // The "1234etc" string form is a 32-hexadecimal-digit representation of the
  // 128-bit base::Token. It may optionally have a filename extension (a suffix
  // that starts with a "." dot) that is ignored by the moniker system but can
  // be useful for moniker consumers that use the filename extension as a hint
  // for how to interpret the byte contents. Ignored by the moniker system
  // means that these inputs produce the same ExtractTokenResult (they all name
  // the same Moniker):
  //
  //  - "moniker/12345678901234567890123456789012"
  //  - "moniker/12345678901234567890123456789012.html"
  //  - "moniker/12345678901234567890123456789012.tar.gz"
  //
  // The argument name is "fs_url_etc", as in storage::FileSystemURL, for
  // historical reasons, even though it is a relative path, not a FileSystemURL
  // (in string form) any more.
  //
  // This function does not resolve the base::Token (for that, use the Resolve
  // function instead). It does not confirm the token's *validity* (that the
  // token matches a previous call to CreateMoniker), only its
  // *well-formed-ness* (that it looks like a token, at the lexical level).
  static ExtractTokenResult ExtractToken(const std::string& fs_url_as_string);

  // Returns the moniker's "/media/fuse/fusebox/moniker/1234etc" filename.
  static std::string GetFilename(const Moniker& moniker);

  MonikerMap();
  MonikerMap(const MonikerMap&) = delete;
  MonikerMap& operator=(const MonikerMap&) = delete;
  ~MonikerMap();

  // Creates a randomly generated link name (available in both base::Token and
  // string form) for the target. It is the caller's responsibility to call
  // DestroyMoniker when the moniker is no longer required but also to keep the
  // FileSystemURL's backing content alive until that DestroyMoniker call.
  Moniker CreateMoniker(const storage::FileSystemURL& target, bool read_only);

  // Tears down the link, so that Resolve will return invalid FileSystemURL
  // values.
  void DestroyMoniker(const Moniker& moniker);

  // Returns the target for the previously created moniker, as identified by
  // its base::Token. The return value's storage::FileSystemURL element
  // is_valid() will be false if there was no such moniker or if it was
  // destroyed. If valid, the bool element is the read_only argument passed to
  // CreateMoniker.
  FSURLAndReadOnlyState Resolve(const Moniker& moniker) const;

  // Returns human-readable debugging information as a JSON value.
  base::Value GetDebugJSON();

 private:
  std::map<base::Token, FSURLAndReadOnlyState> map_;
};

}  // namespace fusebox

#endif  // CHROME_BROWSER_ASH_FUSEBOX_FUSEBOX_MONIKER_H_
