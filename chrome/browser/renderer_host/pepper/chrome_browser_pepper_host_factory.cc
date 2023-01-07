// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/renderer_host/pepper/chrome_browser_pepper_host_factory.h"

#include "build/build_config.h"
#include "chrome/browser/renderer_host/pepper/pepper_isolated_file_system_message_filter.h"
#include "content/public/browser/browser_ppapi_host.h"
#include "ppapi/host/message_filter_host.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_permissions.h"

using ppapi::host::MessageFilterHost;
using ppapi::host::ResourceHost;
using ppapi::host::ResourceMessageFilter;

ChromeBrowserPepperHostFactory::ChromeBrowserPepperHostFactory(
    content::BrowserPpapiHost* host)
    : host_(host) {}

ChromeBrowserPepperHostFactory::~ChromeBrowserPepperHostFactory() {}

std::unique_ptr<ResourceHost>
ChromeBrowserPepperHostFactory::CreateResourceHost(
    ppapi::host::PpapiHost* host,
    PP_Resource resource,
    PP_Instance instance,
    const IPC::Message& message) {
  DCHECK(host == host_->GetPpapiHost());

  // Make sure the plugin is giving us a valid instance for this resource.
  if (!host_->IsValidInstance(instance))
    return nullptr;

  // Permissions for the following interfaces will be checked at the
  // time of the corresponding instance's methods calls (because
  // permission check can be performed only on the UI
  // thread). Currently these interfaces are available only for
  // whitelisted apps which may not have access to the other private
  // interfaces.
  if (message.type() != PpapiHostMsg_IsolatedFileSystem_Create::ID)
    return nullptr;

  PepperIsolatedFileSystemMessageFilter* isolated_fs_filter =
      PepperIsolatedFileSystemMessageFilter::Create(instance, host_);
  if (!isolated_fs_filter)
    return nullptr;
  return std::make_unique<MessageFilterHost>(host, instance, resource,
                                             isolated_fs_filter);
}
