// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_EXTERNAL_PROTOCOL_HANDLER_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_EXTERNAL_PROTOCOL_HANDLER_H_

#include <optional>
#include <string_view>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "url/gurl.h"

class Profile;

namespace guest_os {

// Callback for a custom URL protocol handler (typically an app).
class GuestOsUrlHandler {
 public:
  using HandlerCallback = base::RepeatingCallback<void(Profile*, const GURL&)>;

  // Returns handler for `url` if one exists.
  static std::optional<GuestOsUrlHandler> GetForUrl(Profile* profile,
                                                    const GURL& url);

  GuestOsUrlHandler(std::string_view name, const HandlerCallback handler);
  GuestOsUrlHandler(const GuestOsUrlHandler& other);
  ~GuestOsUrlHandler();

  // Localized name, shown to users when asking whether to use this handler.
  const std::string& name() const { return name_; }

  // Handle the given `url`, for example by launching an app.
  void Handle(Profile* profile, const GURL& url);

 private:
  const std::string name_;
  const HandlerCallback handler_;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_EXTERNAL_PROTOCOL_HANDLER_H_
