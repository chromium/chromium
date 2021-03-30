// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDER_CONFIG_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDER_CONFIG_H_

#include <map>
#include <vector>

#include "base/values.h"

namespace enterprise_connectors {

// The configuration used to describe available service providers for
// Connectors. This configs holds the tags to use, supported mime types for each
// tag, URLs to use for analysis/reporting and other information for each
// service provider.
class ServiceProviderConfig {
 public:
  // Configurations specific to a single service provider.
  class ServiceProvider {
   public:
    class Tag {
     public:
      explicit Tag(const base::Value& tag_value);
      Tag(Tag&&);
      ~Tag();

      const std::vector<std::string>& mime_types() const { return mime_types_; }
      size_t max_file_size() const { return max_file_size_; }

     private:
      std::vector<std::string> mime_types_;
      size_t max_file_size_;
    };

    explicit ServiceProvider(const base::Value& config);
    ServiceProvider(ServiceProvider&&);
    ~ServiceProvider();

    // Used with AnalysisConnector.
    const std::string& analysis_url() const { return analysis_url_; }
    const std::map<std::string, Tag>& analysis_tags() const {
      return analysis_tags_;
    }

    // Used with ReportingConnector.
    const std::string& reporting_url() const { return reporting_url_; }

    // Used with FileSystemConnector.
    const std::string& fs_home_url() const { return fs_home_url_; }
    const std::string& fs_authorization_endpoint() const {
      return fs_authorization_endpoint_;
    }
    const std::string& fs_token_endpoint() const { return fs_token_endpoint_; }
    size_t fs_max_direct_size() const { return fs_max_direct_size_; }
    const std::string& fs_client_id() const { return fs_client_id_; }
    const std::string& fs_client_secret() const { return fs_client_secret_; }
    const std::vector<std::string>& fs_scopes() const { return fs_scopes_; }
    const std::vector<std::string>& fs_disable() const { return fs_disable_; }

   private:
    // Used with AnalysisConnector.
    std::string analysis_url_;
    std::map<std::string, Tag> analysis_tags_;

    // Used with ReportingConnector.
    std::string reporting_url_;

    // Used with FileSystemConnector.
    std::string fs_home_url_;
    std::string fs_authorization_endpoint_;
    std::string fs_token_endpoint_;
    size_t fs_max_direct_size_;
    std::string fs_client_id_;
    std::string fs_client_secret_;
    std::vector<std::string> fs_scopes_;
    std::vector<std::string> fs_disable_;
  };

  explicit ServiceProviderConfig(const std::string& config);
  ServiceProviderConfig(ServiceProviderConfig&&);
  ~ServiceProviderConfig();

  // Returns the names of all registered service providers.
  std::vector<std::string> GetServiceProviderNames() const;

  // Returns the matching service provider, or nullptr if it can't be found.
  const ServiceProvider* GetServiceProvider(
      const std::string& service_provider) const;

 private:
  // Providers known to this config. The key is the service provider name used
  // by the Connector policies.
  std::map<std::string, ServiceProvider> service_providers_;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_SERVICE_PROVIDER_CONFIG_H_
