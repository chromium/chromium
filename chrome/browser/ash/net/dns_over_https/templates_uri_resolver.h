// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_DNS_OVER_HTTPS_TEMPLATES_URI_RESOLVER_H_
#define CHROME_BROWSER_ASH_NET_DNS_OVER_HTTPS_TEMPLATES_URI_RESOLVER_H_

#include <memory>
#include <string>

class PrefService;

namespace ash::dns_over_https {

// An interface used to retrieve the DNS over HTTPS (DoH) provider
// URLs to be used for DNS queries.
class TemplatesUriResolver {
 public:
  TemplatesUriResolver() = default;
  virtual ~TemplatesUriResolver() = default;

  // Evaluates the effective and the display secure DNS templates URI from the
  // pref values of `pref_service` at the time of execution of this function.
  // The results of the last evaluation are available through the getter methods
  // of the class.
  virtual void Update(PrefService* pref_service) = 0;

  // Checks whether the DoH system is configured to provide DoH identifiers in
  // the DNS URL.
  virtual bool GetDohWithIdentifiersActive() = 0;

  // Returns the templates with identifiers to be used for actual DNS lookups.
  // The identifiers are hashed using a salt value.
  virtual std::string GetEffectiveTemplates() = 0;

  // This method is similar to `GetEffectiveTemplates`, but the identifiers are
  // displayed in plain text. Used to display the templates URI in the UI.
  virtual std::string GetDisplayTemplates() = 0;
};

}  // namespace ash::dns_over_https

#endif  // CHROME_BROWSER_ASH_NET_DNS_OVER_HTTPS_TEMPLATES_URI_RESOLVER_H_
