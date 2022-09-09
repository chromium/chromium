// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_ALL_SIGNAL_H_
#define CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_ALL_SIGNAL_H_

#include "chrome/browser/safe_browsing/extension_telemetry/extension_signal.h"

namespace safe_browsing {

// A signal that is created when an extension invokes cookies.getAll API.
class CookiesGetAllSignal : public ExtensionSignal {
 public:
  CookiesGetAllSignal(const extensions::ExtensionId& extension_id,
                      const std::string& domain,
                      const std::string& name,
                      const std::string& path,
                      bool secure,
                      const std::string& store_id,
                      const std::string& url,
                      bool is_session);
  ~CookiesGetAllSignal() override;

  // ExtensionSignal:
  ExtensionSignalType GetType() const override;

  // Creates a unique id, which can be used to compare argument sets and as a
  // key for storage (e.g., in a map).
  std::string getUniqueArgSetId() const;

  const std::string& domain() const { return domain_; }
  const std::string& name() const { return name_; }
  const std::string& path() const { return path_; }
  bool secure() const { return secure_; }
  const std::string& store_id() const { return store_id_; }
  const std::string& url() const { return url_; }
  bool is_session() const { return is_session_; }

 protected:
  // Restricts the retrieved cookies to those whose domains match or are
  // subdomains of this one.
  std::string domain_;
  // Filters the cookies by name.
  std::string name_;
  // Restricts the retrieved cookies to those whose path exactly matches this
  // string.
  std::string path_;
  // Filters the cookies by their Secure property.
  bool secure_;
  // The cookie store to retrieve cookies from. If omitted, the current
  // execution context's cookie store will be used.
  std::string store_id_;
  // Restricts the retrieved cookies to those that would match the given URL.
  std::string url_;
  // Filters out session vs.persistent cookies.
  bool is_session_;
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_EXTENSION_TELEMETRY_COOKIES_GET_ALL_SIGNAL_H_
