// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/chrome_extensions_api_client.h"

#include "base/notimplemented.h"
#include "extensions/browser/api/messaging/messaging_delegate.h"
#include "extensions/browser/api/messaging/native_message_host.h"
#include "extensions/buildflags/buildflags.h"

// TODO(crbug.com/417770773): This file contains stubs for the parts of
// ChromeExtensionsAPIClient that are not yet supported on desktop Android. Once
// these functions are supported on desktop Android this file can be deleted.
// The stubs are implemented here instead of falling back to ExtensionsAPIClient
// to allow NOTIMPLEMENTED() logging and a place to put TODOs with bug IDs.

static_assert(BUILDFLAG(ENABLE_DESKTOP_ANDROID_EXTENSIONS));

namespace extensions {

void ChromeExtensionsAPIClient::OpenFileUrl(
    const GURL& file_url,
    content::BrowserContext* browser_context) {
  // TODO(crbug.com/417785325): Support opening file URLs on desktop Android.
  NOTIMPLEMENTED();
}

std::unique_ptr<DevicePermissionsPrompt>
ChromeExtensionsAPIClient::CreateDevicePermissionsPrompt(
    content::WebContents* web_contents) const {
  // TODO(crbug.com/417785993): Support device permissions prompts on desktop
  // Android.
  return nullptr;
}

std::unique_ptr<SupervisedUserExtensionsDelegate>
ChromeExtensionsAPIClient::CreateSupervisedUserExtensionsDelegate(
    content::BrowserContext* browser_context) const {
  // TODO(crbug.com/402488726): Support supervised users on desktop Android.
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<DisplayInfoProvider>
ChromeExtensionsAPIClient::CreateDisplayInfoProvider() const {
  // TODO(crbug.com/417786011): Support display APIs on desktop Android.
  NOTIMPLEMENTED();
  return nullptr;
}

MessagingDelegate* ChromeExtensionsAPIClient::GetMessagingDelegate() {
  if (!messaging_delegate_) {
    // The default implementation does nothing, which is fine for now, since
    // this is mostly needed for:
    //   a) tab-specifics,
    //   b) platform apps, and
    //   c) native messaging
    // TODO(crbug.com/371432155): Use ChromeMessagingDelegate when we have
    // better support for tabs.
    messaging_delegate_ = std::make_unique<MessagingDelegate>();
  }
  return messaging_delegate_.get();
}

std::vector<KeyedServiceBaseFactory*>
ChromeExtensionsAPIClient::GetFactoryDependencies() {
  // TODO(crbug.com/402488726): Delete this stub and use the version from
  // _non_android.cc when we have supervised user support on desktop Android.
  // Don't use NOTIMPLEMENTED() here because this is the correct implementation
  // for this stub class.
  return {};
}

std::unique_ptr<NativeMessagePortDispatcher>
ChromeExtensionsAPIClient::CreateNativeMessagePortDispatcher(
    std::unique_ptr<NativeMessageHost> host,
    base::WeakPtr<NativeMessagePort> port,
    scoped_refptr<base::SingleThreadTaskRunner> message_service_task_runner) {
  // TODO(crbug.com/417786914): Support native messaging on desktop Android.
  return nullptr;
}

}  // namespace extensions
