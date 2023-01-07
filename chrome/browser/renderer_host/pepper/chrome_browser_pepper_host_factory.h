// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RENDERER_HOST_PEPPER_CHROME_BROWSER_PEPPER_HOST_FACTORY_H_
#define CHROME_BROWSER_RENDERER_HOST_PEPPER_CHROME_BROWSER_PEPPER_HOST_FACTORY_H_

#include "base/memory/raw_ptr.h"
#include "ppapi/host/host_factory.h"

namespace content {
class BrowserPpapiHost;
}  // namespace content

class ChromeBrowserPepperHostFactory : public ppapi::host::HostFactory {
 public:
  // Non-owning pointer to the filter must outlive this class.
  explicit ChromeBrowserPepperHostFactory(content::BrowserPpapiHost* host);

  ChromeBrowserPepperHostFactory(const ChromeBrowserPepperHostFactory&) =
      delete;
  ChromeBrowserPepperHostFactory& operator=(
      const ChromeBrowserPepperHostFactory&) = delete;

  ~ChromeBrowserPepperHostFactory() override;

  std::unique_ptr<ppapi::host::ResourceHost> CreateResourceHost(
      ppapi::host::PpapiHost* host,
      PP_Resource resource,
      PP_Instance instance,
      const IPC::Message& message) override;

 private:
  // Non-owning pointer.
  raw_ptr<content::BrowserPpapiHost> host_;
};

#endif  // CHROME_BROWSER_RENDERER_HOST_PEPPER_CHROME_BROWSER_PEPPER_HOST_FACTORY_H_
