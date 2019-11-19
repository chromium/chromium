// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/smb_url.h"

#include <vector>

#include "base/strings/strcat.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/chromeos/smb_client/smb_constants.h"
#include "url/url_canon_stdstring.h"

namespace chromeos {
namespace smb_client {

namespace {

const char kSingleBackslash[] = "\\";
const char kDoubleBackslash[] = "\\\\";

// Returns true if |url| starts with "smb://" or "\\".
bool ShouldProcessUrl(const std::string& url) {
  return base::StartsWith(url, kSmbSchemePrefix,
                          base::CompareCase::INSENSITIVE_ASCII) ||
         base::StartsWith(url, kDoubleBackslash,
                          base::CompareCase::INSENSITIVE_ASCII);
}

// Adds "smb://" to the beginning of |url| if not present.
std::string AddSmbSchemeIfMissing(const std::string& url) {
  DCHECK(ShouldProcessUrl(url));

  if (base::StartsWith(url, kSmbSchemePrefix,
                       base::CompareCase::INSENSITIVE_ASCII)) {
    return url;
  }

  return std::string(kSmbSchemePrefix) + url;
}

// Returns true if |parsed| contains a username, password, port, query, or ref.
bool ContainsUnnecessaryComponents(const url::Parsed& parsed) {
  return parsed.username.is_nonempty() || parsed.password.is_nonempty() ||
         parsed.port.is_nonempty() || parsed.query.is_nonempty() ||
         parsed.ref.is_nonempty();
}

// Parses |url| into |parsed|. Returns true if the URL does not contain
// unnecessary components.
bool ParseAndValidateUrl(const std::string& url, url::Parsed* parsed) {
  DCHECK(parsed);
  DCHECK(ShouldProcessUrl(url));

  url::ParseStandardURL(url.c_str(), url.size(), parsed);
  return !ContainsUnnecessaryComponents(*parsed);
}

}  // namespace

SmbUrl::SmbUrl(const std::string& raw_url) {
  // Only process |url| if it starts with "smb://" or "\\".
  if (ShouldProcessUrl(raw_url)) {
    // Add "smb://" if |url| starts with "\\" and canonicalize the URL.
    CanonicalizeSmbUrl(AddSmbSchemeIfMissing(raw_url));
    // Create the Windows UNC for the url.
    CreateWindowsUnc(raw_url);
  }
}

SmbUrl::~SmbUrl() = default;

SmbUrl::SmbUrl(SmbUrl&& smb_url) = default;

std::string SmbUrl::GetHost() const {
  DCHECK(IsValid());

  return url_.substr(host_.begin, host_.len);
}

std::string SmbUrl::GetShare() const {
  DCHECK(IsValid());

  return share_;
}

const std::string& SmbUrl::ToString() const {
  DCHECK(IsValid());

  return url_;
}

std::string SmbUrl::ReplaceHost(const std::string& new_host) const {
  DCHECK(IsValid());

  std::string temp = url_;
  temp.replace(host_.begin, host_.len, new_host);
  return temp;
}

bool SmbUrl::IsValid() const {
  return !url_.empty() && host_.is_valid();
}

std::string SmbUrl::GetWindowsUNCString() const {
  DCHECK(IsValid());

  return windows_unc_;
}

void SmbUrl::CanonicalizeSmbUrl(const std::string& url) {
  DCHECK(!IsValid());
  DCHECK(ShouldProcessUrl(url));

  // Get the initial parse of |url| and ensure that it does not contain
  // unnecessary components.
  url::Parsed initial_parsed;
  if (!ParseAndValidateUrl(url, &initial_parsed)) {
    return;
  }

  url::StdStringCanonOutput canonical_output(&url_);

  url::Component scheme;
  if (!url::CanonicalizeScheme(url.c_str(), initial_parsed.scheme,
                               &canonical_output, &scheme)) {
    Reset();
    return;
  }

  canonical_output.push_back('/');
  canonical_output.push_back('/');

  url::Component path;
  if (!(url::CanonicalizeHost(url.c_str(), initial_parsed.host,
                              &canonical_output, &host_) &&
        url::CanonicalizePath(url.c_str(), initial_parsed.path,
                              &canonical_output, &path))) {
    Reset();
    return;
  }

  // Check IsValid here since url::Canonicalize* may return true even if it did
  // not actually parse the component.
  if (!IsValid()) {
    Reset();
    return;
  }

  canonical_output.Complete();

  if (path.is_nonempty()) {
    // Extract share name, which is the first path element.
    // Paths always start with '/', but extra '/'s are not removed.
    // So both "smb://foo" and "smb://foo//bar/" have the share name "", but
    // "smb://foo/bar/" has the share name "bar".
    std::string path_str = url_.substr(path.begin, path.len);
    std::vector<base::StringPiece> split_path = base::SplitStringPiece(
        path_str, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
    if (split_path.size() >= 2) {
      DCHECK_EQ(split_path[0], "");
      share_ = split_path[1].as_string();
    }
  }

  DCHECK(host_.is_nonempty());
  DCHECK_EQ(url_.substr(scheme.begin, scheme.len), kSmbScheme);
}

void SmbUrl::CreateWindowsUnc(const std::string& url) {
  url::Parsed parsed;
  if (!ParseAndValidateUrl(url, &parsed)) {
    return;
  }

  const std::string host = url.substr(parsed.host.begin, parsed.host.len);
  std::string path = url.substr(parsed.path.begin, parsed.path.len);

  // Turn any forward slashes into escaped backslashes.
  base::ReplaceChars(path, "/", kSingleBackslash, &path);

  windows_unc_ = base::StrCat({kDoubleBackslash, host, path});
}

void SmbUrl::Reset() {
  host_.reset();
  url_.clear();
}

}  // namespace smb_client
}  // namespace chromeos
