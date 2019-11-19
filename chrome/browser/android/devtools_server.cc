// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/devtools_server.h"

#include <pwd.h>
#include <cstring>
#include <utility>

#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/android/chrome_jni_headers/DevToolsServer_jni.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/android/tab_model/tab_model.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/common/chrome_content_client.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "net/base/net_errors.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#include "net/url_request/url_request_context_getter.h"

using base::android::JavaParamRef;
using content::DevToolsAgentHost;
using content::RenderViewHost;
using content::WebContents;

namespace {

// TL;DR: Do not change this string.
//
// Desktop Chrome relies on this format to identify debuggable apps on Android
// (see the code under chrome/browser/devtools/device).
// If this string ever changes it would not be sufficient to change the
// corresponding string on the client side. Since debugging an older version of
// Chrome for Android from a newer version of desktop Chrome is a very common
// scenario, the client code will have to be modified to recognize both the old
// and the new format.
const char kDevToolsChannelNameFormat[] = "%s_devtools_remote";

const char kTetheringSocketName[] = "chrome_devtools_tethering_%d_%d";

const int kBackLog = 10;

bool AuthorizeSocketAccessWithDebugPermission(
    const net::UnixDomainServerSocket::Credentials& credentials) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_DevToolsServer_checkDebugPermission(env, credentials.process_id,
                                                  credentials.user_id) ||
         content::CanUserConnectToDevTools(credentials);
}

// Factory for UnixDomainServerSocket. It tries a fallback socket when
// original socket doesn't work.
class UnixDomainServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  UnixDomainServerSocketFactory(
      const std::string& socket_name,
      const net::UnixDomainServerSocket::AuthCallback& auth_callback)
      : socket_name_(socket_name),
        last_tethering_socket_(0),
        auth_callback_(auth_callback) {
  }

 private:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(auth_callback_,
                                        true /* use_abstract_namespace */));

    if (socket->BindAndListen(socket_name_, kBackLog) == net::OK)
      return std::move(socket);

    // Try a fallback socket name.
    const std::string fallback_address(
        base::StringPrintf("%s_%d", socket_name_.c_str(), getpid()));
    if (socket->BindAndListen(fallback_address, kBackLog) == net::OK)
      return std::move(socket);

    return std::unique_ptr<net::ServerSocket>();
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    *name = base::StringPrintf(
        kTetheringSocketName, getpid(), ++last_tethering_socket_);
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(auth_callback_, true));
    if (socket->BindAndListen(*name, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    return std::move(socket);
  }

  std::string socket_name_;
  int last_tethering_socket_;
  net::UnixDomainServerSocket::AuthCallback auth_callback_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};

}  // namespace

DevToolsServer::DevToolsServer(const std::string& socket_name_prefix)
    : socket_name_(base::StringPrintf(kDevToolsChannelNameFormat,
                                      socket_name_prefix.c_str())),
      is_started_(false) {
  // Override the socket name if one is specified on the command line.
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  if (command_line.HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name_ = command_line.GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
}

DevToolsServer::~DevToolsServer() {
  Stop();
}

void DevToolsServer::Start(bool allow_debug_permission) {
  if (is_started_)
    return;

  net::UnixDomainServerSocket::AuthCallback auth_callback =
      allow_debug_permission ?
          base::Bind(&AuthorizeSocketAccessWithDebugPermission) :
          base::Bind(&content::CanUserConnectToDevTools);
  std::unique_ptr<content::DevToolsSocketFactory> factory(
      new UnixDomainServerSocketFactory(socket_name_, auth_callback));
  DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(factory),
      base::FilePath(), base::FilePath());
  is_started_ = true;
}

void DevToolsServer::Stop() {
  is_started_ = false;
  DevToolsAgentHost::StopRemoteDebuggingServer();
}

bool DevToolsServer::IsStarted() const {
  return is_started_;
}

static jlong JNI_DevToolsServer_InitRemoteDebugging(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& socket_name_prefix) {
  DevToolsServer* server = new DevToolsServer(
      base::android::ConvertJavaStringToUTF8(env, socket_name_prefix));
  return reinterpret_cast<intptr_t>(server);
}

static void JNI_DevToolsServer_DestroyRemoteDebugging(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong server) {
  delete reinterpret_cast<DevToolsServer*>(server);
}

static jboolean JNI_DevToolsServer_IsRemoteDebuggingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong server) {
  return reinterpret_cast<DevToolsServer*>(server)->IsStarted();
}

static void JNI_DevToolsServer_SetRemoteDebuggingEnabled(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jlong server,
    jboolean enabled,
    jboolean allow_debug_permission) {
  DevToolsServer* devtools_server = reinterpret_cast<DevToolsServer*>(server);
  if (enabled) {
    devtools_server->Start(allow_debug_permission);
  } else {
    devtools_server->Stop();
  }
}
