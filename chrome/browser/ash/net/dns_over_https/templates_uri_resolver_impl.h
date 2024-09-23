// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_DNS_OVER_HTTPS_TEMPLATES_URI_RESOLVER_IMPL_H_
#define CHROME_BROWSER_ASH_NET_DNS_OVER_HTTPS_TEMPLATES_URI_RESOLVER_IMPL_H_

#include <memory>
#include <string>
#include <string_view>

#include "chrome/browser/ash/net/dns_over_https/templates_uri_resolver.h"
#include "chrome/browser/ash/policy/core/device_attributes.h"
#include "components/prefs/pref_change_registrar.h"

namespace policy {
class DeviceAttributes;
class FakeDeviceAttributes;
}  // namespace policy

class PrefService;

namespace ash::dns_over_https {

// A helper class which is used to retrieve the DNS over HTTPS (DoH) provider
// URLs to be used for DNS queries.
class TemplatesUriResolverImpl : public TemplatesUriResolver {
 public:
  // The constructor retrieves all prefs and checks if identifiers are enabled,
  // constructs the effective URL and display URL lists.
  //
  // This is based on several prefs (kDnsOverHttpsTemplates,
  // kDnsOverHttpsTemplatesWithIdentifiers, kDnsOverHttpsSalt), which can be set
  // from policies or user input.
  //
  // The value of kDnsOverHttpsTemplatesWithIdentifiers will overwrite the
  // kDnsOverHttpsTemplates value if given (and non-empty) and if there is also
  // a salt for hashing identifier values to keep them from any eavesdropper.
  // Each identifier occurrence will be replaced by hash(salt + value). This
  // class is Chrome OS only and on other platforms only kDnsOverHttpsTemplates
  // can be set.
  TemplatesUriResolverImpl();
  TemplatesUriResolverImpl(const TemplatesUriResolverImpl&) = delete;
  TemplatesUriResolverImpl& operator=(const TemplatesUriResolverImpl&) = delete;
  ~TemplatesUriResolverImpl() override;

  // TemplatesUriResolver implementation.
  void Update(PrefService* pref_service) override;

  // This function checks whether the DoH system is configured to provide
  // DoH identifiers in the DNS URL
  bool GetDohWithIdentifiersActive() override;

  // This function returns the templates to be used for actual DNS lookups
  std::string GetEffectiveTemplates() override;

  // This function is very similar to the above, except that a variable will
  // be replaced by "${<variable_value>}" for display purposes instead of being
  // hashed.
  std::string GetDisplayTemplates() override;

  void SetDeviceAttributesForTesting(
      std::unique_ptr<policy::FakeDeviceAttributes> attributes);

  // Indicates if `uri_templates` contains the template URI placeholder for the
  // device IP addresses, as defined by the policy
  // DnsOverHttpsTemplatesWithIdentifiers.
  static bool IsDeviceIpAddressIncludedInUriTemplate(
      std::string_view uri_templates);

 private:
  bool doh_with_identifiers_active_ = false;
  std::string effective_templates_;
  std::string display_templates_;
  std::unique_ptr<policy::DeviceAttributes> attributes_;
};

}  // namespace ash::dns_over_https

#endif  // CHROME_BROWSER_ASH_NET_DNS_OVER_HTTPS_TEMPLATES_URI_RESOLVER_IMPL_H_
