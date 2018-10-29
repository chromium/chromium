// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_SERVICE_H_
#define CHROME_BROWSER_CHROME_SERVICE_H_

#include "base/no_destructor.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "services/service_manager/public/cpp/embedded_service_info.h"

namespace service_manager {
class Connector;
class Service;
}  // namespace service_manager

class ChromeBrowserMainExtraParts;

// Provides access to a service for the "chrome" content embedder. Actual
// service_manager::Service implementation lives on IO thread (IOThreadContext).
class ChromeService {
 public:
  static ChromeService* GetInstance();

  // ChromeBrowserMain takes ownership of the returned parts.
  ChromeBrowserMainExtraParts* CreateExtraParts();

  service_manager::EmbeddedServiceInfo::ServiceFactory
  CreateChromeServiceFactory();

  // This is available after the content::ServiceManagerConnection is
  // initialized.
  service_manager::Connector* connector() { return connector_.get(); }

 private:
  friend class base::NoDestructor<ChromeService>;

  class ExtraParts;
  class IOThreadContext;

  ChromeService();
  ~ChromeService();

  void InitConnector();

  std::unique_ptr<service_manager::Service> CreateChromeServiceWrapper();

  const std::unique_ptr<IOThreadContext> io_thread_context_;

  std::unique_ptr<service_manager::Connector> connector_;

  DISALLOW_COPY_AND_ASSIGN(ChromeService);
};

#endif  // CHROME_BROWSER_CHROME_SERVICE_H_
