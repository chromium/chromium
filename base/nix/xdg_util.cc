// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/nix/xdg_util.h"

#include <optional>
#include <string>
#include <utility>

#include "base/base_paths.h"
#include "base/command_line.h"
#include "base/environment.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/third_party/xdg_user_dirs/xdg_user_dir_lookup.h"
#include "base/threading/scoped_blocking_call.h"

namespace {

// The KDE session version environment variable introduced in KDE 4.
const char kKDESessionEnvVar[] = "KDE_SESSION_VERSION";

base::nix::XdgActivationTokenCreator& GetXdgActivationTokenCreator() {
  static base::NoDestructor<base::nix::XdgActivationTokenCreator> creator;
  return *creator;
}

std::optional<std::string>& GetXdgActivationToken() {
  static base::NoDestructor<std::optional<std::string>> token;
  return *token;
}

}  // namespace

namespace base::nix {

FilePath GetXDGDirectory(Environment* env,
                         cstring_view env_name,
                         const char* fallback_dir) {
  FilePath path;
  if (auto env_value = env->GetVar(env_name).value_or(""); !env_value.empty()) {
    path = FilePath(env_value);
  } else {
    PathService::Get(DIR_HOME, &path);
    path = path.Append(fallback_dir);
  }
  return path.StripTrailingSeparators();
}

FilePath GetXDGUserDirectory(const char* dir_name, const char* fallback_dir) {
  FilePath path;
  char* xdg_dir = xdg_user_dir_lookup(dir_name);
  if (xdg_dir) {
    path = FilePath(xdg_dir);
    free(xdg_dir);
  } else {
    PathService::Get(DIR_HOME, &path);
    path = path.Append(fallback_dir);
  }
  return path.StripTrailingSeparators();
}

FilePath GetXDGDataWriteLocation(Environment* env) {
  return GetXDGDirectory(env, "XDG_DATA_HOME", ".local/share");
}

std::vector<FilePath> GetXDGDataSearchLocations(Environment* env) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  std::vector<FilePath> search_paths;
  search_paths.push_back(GetXDGDataWriteLocation(env));

  if (auto xdg_data_dirs = env->GetVar("XDG_DATA_DIRS").value_or("");
      !xdg_data_dirs.empty()) {
    StringTokenizer tokenizer(xdg_data_dirs, ":");
    while (tokenizer.GetNext()) {
      search_paths.emplace_back(tokenizer.token_piece());
    }
  } else {
    search_paths.emplace_back("/usr/local/share");
    search_paths.emplace_back("/usr/share");
  }

  return search_paths;
}

DesktopEnvironment GetDesktopEnvironment(Environment* env) {
  // kXdgCurrentDesktopEnvVar is the newest standard circa 2012.
  if (std::optional<std::string> maybe_xdg_current_desktop =
          env->GetVar(kXdgCurrentDesktopEnvVar)) {
    const std::string& xdg_current_desktop = maybe_xdg_current_desktop.value();
    // It could have multiple values separated by colon in priority order.
    for (const auto& value : SplitStringPiece(
             xdg_current_desktop, ":", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY)) {
      if (value == "Unity") {
        // gnome-fallback sessions set kXdgCurrentDesktopEnvVar to Unity
        // DESKTOP_SESSION can be gnome-fallback or gnome-fallback-compiz
        std::string desktop_session =
            env->GetVar("DESKTOP_SESSION").value_or("");
        if (desktop_session.find("gnome-fallback") != std::string::npos) {
          return DESKTOP_ENVIRONMENT_GNOME;
        }
        return DESKTOP_ENVIRONMENT_UNITY;
      }
      if (value == "Deepin") {
        return DESKTOP_ENVIRONMENT_DEEPIN;
      }
      if (value == "GNOME") {
        return DESKTOP_ENVIRONMENT_GNOME;
      }
      if (value == "X-Cinnamon") {
        return DESKTOP_ENVIRONMENT_CINNAMON;
      }
      if (value == "KDE") {
        std::optional<std::string> kde_session =
            env->GetVar(kKDESessionEnvVar).value_or("");
        if (kde_session == "5") {
          return DESKTOP_ENVIRONMENT_KDE5;
        }
          if (kde_session == "6") {
            return DESKTOP_ENVIRONMENT_KDE6;
          }

        return DESKTOP_ENVIRONMENT_KDE4;
      }
      if (value == "Pantheon") {
        return DESKTOP_ENVIRONMENT_PANTHEON;
      }
      if (value == "XFCE") {
        return DESKTOP_ENVIRONMENT_XFCE;
      }
      if (value == "UKUI") {
        return DESKTOP_ENVIRONMENT_UKUI;
      }
      if (value == "LXQt") {
        return DESKTOP_ENVIRONMENT_LXQT;
      }
    }
  }

  // DESKTOP_SESSION was what everyone used in 2010.
  std::string desktop_session = env->GetVar("DESKTOP_SESSION").value_or("");
  if (desktop_session == "deepin") {
    return DESKTOP_ENVIRONMENT_DEEPIN;
  }
  if (desktop_session == "gnome" || desktop_session == "mate") {
    return DESKTOP_ENVIRONMENT_GNOME;
  }
  if (desktop_session == "kde4" || desktop_session == "kde-plasma") {
    return DESKTOP_ENVIRONMENT_KDE4;
  }
  if (desktop_session == "kde") {
    // This may mean KDE4 on newer systems, so we have to check.
    if (env->HasVar(kKDESessionEnvVar)) {
      return DESKTOP_ENVIRONMENT_KDE4;
    }
    return DESKTOP_ENVIRONMENT_KDE3;
  }
  if (desktop_session.find("xfce") != std::string::npos ||
      desktop_session == "xubuntu") {
    return DESKTOP_ENVIRONMENT_XFCE;
  }
  if (desktop_session == "ukui") {
    return DESKTOP_ENVIRONMENT_UKUI;
  }

  // Fall back on some older environment variables.
  // Useful particularly in the DESKTOP_SESSION=default case.
  if (env->HasVar("GNOME_DESKTOP_SESSION_ID")) {
    return DESKTOP_ENVIRONMENT_GNOME;
  }
  if (env->HasVar("KDE_FULL_SESSION")) {
    if (env->HasVar(kKDESessionEnvVar)) {
      return DESKTOP_ENVIRONMENT_KDE4;
    }
    return DESKTOP_ENVIRONMENT_KDE3;
  }

  return DESKTOP_ENVIRONMENT_OTHER;
}

const char* GetDesktopEnvironmentName(DesktopEnvironment env) {
  switch (env) {
    case DESKTOP_ENVIRONMENT_OTHER:
      return nullptr;
    case DESKTOP_ENVIRONMENT_CINNAMON:
      return "CINNAMON";
    case DESKTOP_ENVIRONMENT_DEEPIN:
      return "DEEPIN";
    case DESKTOP_ENVIRONMENT_GNOME:
      return "GNOME";
    case DESKTOP_ENVIRONMENT_KDE3:
      return "KDE3";
    case DESKTOP_ENVIRONMENT_KDE4:
      return "KDE4";
    case DESKTOP_ENVIRONMENT_KDE5:
      return "KDE5";
    case DESKTOP_ENVIRONMENT_KDE6:
      return "KDE6";
    case DESKTOP_ENVIRONMENT_PANTHEON:
      return "PANTHEON";
    case DESKTOP_ENVIRONMENT_UNITY:
      return "UNITY";
    case DESKTOP_ENVIRONMENT_XFCE:
      return "XFCE";
    case DESKTOP_ENVIRONMENT_UKUI:
      return "UKUI";
    case DESKTOP_ENVIRONMENT_LXQT:
      return "LXQT";
  }
  return nullptr;
}

const char* GetDesktopEnvironmentName(Environment* env) {
  return GetDesktopEnvironmentName(GetDesktopEnvironment(env));
}

SessionType GetSessionType(Environment& env) {
  std::optional<std::string> maybe_xdg_session_type =
      env.GetVar(kXdgSessionTypeEnvVar);
  if (!maybe_xdg_session_type.has_value()) {
    return SessionType::kUnset;
  }

  std::string& xdg_session_type = maybe_xdg_session_type.value();
  TrimWhitespaceASCII(ToLowerASCII(xdg_session_type), TrimPositions::TRIM_ALL,
                      &xdg_session_type);

  if (xdg_session_type == "wayland") {
    return SessionType::kWayland;
  }

  if (xdg_session_type == "x11") {
    return SessionType::kX11;
  }

  if (xdg_session_type == "tty") {
    return SessionType::kTty;
  }

  if (xdg_session_type == "mir") {
    return SessionType::kMir;
  }

  if (xdg_session_type == "unspecified") {
    return SessionType::kUnspecified;
  }

  LOG(ERROR) << "Unknown XDG_SESSION_TYPE: " << xdg_session_type;
  return SessionType::kOther;
}

std::optional<std::string> ExtractXdgActivationTokenFromEnv(Environment& env) {
  std::string token;
  if (token = env.GetVar(kXdgActivationTokenEnvVar).value_or("");
      !token.empty()) {
    GetXdgActivationToken() = std::move(token);
    env.UnSetVar(kXdgActivationTokenEnvVar);
  } else if (token = env.GetVar(kDesktopStartupIdEnvVar).value_or("");
             !token.empty()) {
    // X11 apps use DESKTOP_STARTUP_ID to pass the activation token.
    // https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/staging/xdg-activation/x11-interoperation.rst
    GetXdgActivationToken() = std::move(token);
    env.UnSetVar(kDesktopStartupIdEnvVar);
  }
  return GetXdgActivationToken();
}

void ExtractXdgActivationTokenFromCmdLine(base::CommandLine& cmd_line) {
  std::string token = cmd_line.GetSwitchValueASCII(kXdgActivationTokenSwitch);
  if (!token.empty()) {
    GetXdgActivationToken() = std::move(token);
    cmd_line.RemoveSwitch(kXdgActivationTokenSwitch);
  }
}

void SetActivationToken(std::string token) {
  GetXdgActivationToken() = std::move(token);
}

std::optional<std::string> TakeXdgActivationToken() {
  auto token = GetXdgActivationToken();
  GetXdgActivationToken().reset();
  return token;
}

void SetXdgActivationTokenCreator(XdgActivationTokenCreator token_creator) {
  GetXdgActivationTokenCreator() = std::move(token_creator);
}

void CreateLaunchOptionsWithXdgActivation(
    XdgActivationLaunchOptionsCallback callback) {
  if (!GetXdgActivationTokenCreator()) {
    // There is no token creator, so return an empty LaunchOptions.
    std::move(callback).Run(LaunchOptions());
    return;
  }
  auto create_token_cb =
      [](XdgActivationLaunchOptionsCallback launch_options_cb,
         std::string token) {
        base::LaunchOptions options;
        if (!token.empty()) {
          options.environment[kXdgActivationTokenEnvVar] = token;
        }
        std::move(launch_options_cb).Run(options);
      };
  GetXdgActivationTokenCreator().Run(
      base::BindOnce(create_token_cb, std::move(callback)));
}

void CreateXdgActivationToken(XdgActivationTokenCallback callback) {
  if (!GetXdgActivationTokenCreator()) {
    std::move(callback).Run({});
    return;
  }
  GetXdgActivationTokenCreator().Run(std::move(callback));
}

std::string XdgDesktopPortalRequestPath(const std::string& sender,
                                        const std::string& token) {
  // Since version 0.9 of xdg-desktop-portal, the handle will be of the form
  // /org/freedesktop/portal/desktop/request/SENDER/TOKEN where SENDER is the
  // caller's unique name, with the initial ':' removed and all '.' replaced by
  // '_', and TOKEN is a unique token that the caller provided with the
  // handle_token key in the options vardict. See:
  // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Request.html
  static constexpr char kObjectPathRequestFormat[] =
      "/org/freedesktop/portal/desktop/request/$1/$2";

  auto sender_name =
      !sender.empty() && sender[0] == ':' ? sender.substr(1) : sender;
  std::string bus_name;
  base::ReplaceChars(sender_name, ".", "_", &bus_name);
  return ReplaceStringPlaceholders(kObjectPathRequestFormat,
                                   std::vector<std::string>{bus_name, token},
                                   nullptr);
}

std::string XdgDesktopPortalSessionPath(const std::string& sender,
                                        const std::string& token) {
  // Since version 0.9 of xdg-desktop-portal, the handle will be of the form
  // /org/freedesktop/portal/desktop/session/SENDER/TOKEN where SENDER is the
  // caller's unique name, with the initial ':' removed and all '.' replaced by
  // '_', and TOKEN is a unique token that the caller provided with the
  // handle_token key in the options vardict. See:
  // https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.portal.Session.html
  static constexpr char kObjectPathSessionFormat[] =
      "/org/freedesktop/portal/desktop/session/$1/$2";

  auto sender_name =
      !sender.empty() && sender[0] == ':' ? sender.substr(1) : sender;
  std::string bus_name;
  base::ReplaceChars(sender_name, ".", "_", &bus_name);
  return ReplaceStringPlaceholders(kObjectPathSessionFormat,
                                   std::vector<std::string>{bus_name, token},
                                   nullptr);
}

}  // namespace base::nix
