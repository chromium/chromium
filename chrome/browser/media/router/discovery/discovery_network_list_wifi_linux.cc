// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/router/discovery/discovery_network_list_wifi.h"

#include <linux/wireless.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <array>
#include <optional>
#include <string>
#include <string_view>

#include "base/files/scoped_file.h"
#include "base/strings/string_util.h"
#include "base/strings/string_view_util.h"
#include "net/base/network_interfaces_linux.h"

namespace media_router {

std::optional<std::string> MaybeGetWifiSSID(std::string_view if_name) {
  base::ScopedFD ioctl_socket(socket(AF_INET, SOCK_DGRAM, 0));
  if (!ioctl_socket.is_valid()) {
    // AF_INET is for IPv4, so it may fail for IPv6-only hosts even when there
    // are interfaces up.
    ioctl_socket.reset(socket(AF_INET6, SOCK_DGRAM, 0));
    if (!ioctl_socket.is_valid()) {
      return std::nullopt;
    }
  }
  struct iwreq wreq = {};
  base::strlcpy(wreq.ifr_name, if_name);

  std::array<char, IW_ESSID_MAX_SIZE + 1> ssid = {};
  wreq.u.essid.pointer = ssid.data();
  wreq.u.essid.length = ssid.size() - 1;  // Keep room for the NUL.
  if (ioctl(ioctl_socket.get(), SIOCGIWESSID, &wreq) == -1) {
    return std::nullopt;
  }
  std::string_view ssid_view = base::as_string_view(ssid);
  ssid_view = ssid_view.substr(0, ssid_view.find('\0'));
  if (ssid_view.empty()) {
    return std::nullopt;
  }
  return std::string(ssid_view);
}

}  // namespace media_router
