// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

#include "base/containers/flat_map.h"
#include "components/profile_metrics/browser_profile_type.h"

#include "third_party/abseil-cpp/absl/types/optional.h"

// Purpose of this API:
// Provide a Profile type specific implementation logic for
// `KeyedServiceFactory` under chrome/.
//
// - `ProfileSelection`: Enum used to map the logic of selecting the right
// profile for the service to be created for, based on the given profile.
// - `ProfileSelections`: Helper structure that contains a
// `ProfileSelection` value for each main Profile type (Regular, Guest and
// System).
//     - `ProfileSelections::Builder`: Used to easily create
//     `ProfileSelections`.
// - `ProfileKeyedServiceFactory`: Intermediate Factory class that inherits from
// `BrowserContextKeyedServiceFactory`. Main purpose of this intermediate class
// is to provide an easy and efficient way to provide the redirection logic for
// each main profile types using `ProfileSelections` instance.

class Profile;

// Enum that sets the Profile Redirection logic given a Profile.
enum class ProfileSelection {
  kNone,                  // Original: No Profile  --  OTR: No Profile
  kOriginalOnly,          // Original: Self        --  OTR: No Profile
  kOwnInstance,           // Original: Self        --  OTR: Self
  kRedirectedToOriginal,  // Original: Self        --  OTR: Original
  kOffTheRecordOnly       // Original: No Profile  --  OTR: Self
};

// Contains the logic for ProfileSelection for the different main Profile types
// (Regular, Guest and System). Each of these profile types also have Off the
// Record profiles equivalent, e.g. Incognito is Off the Record profile for
// Regular profile, the Guest user-visible profile is off-the-record, the
// Profile Picker uses the off-the-record System Profile.
// Maps Profile types to `ProfileSelection`.
class ProfileSelections {
 public:
  ProfileSelections(const ProfileSelections& other);
  ~ProfileSelections();

  // - Predefined `ProfileSelections`

  // Default implementation, as of now:
  // - No services in OTR.
  // - Regular profile returns itself (original).
  // - Guest and System profiles follow Regular profile behaviour.
  //
  // After the migration(crbug.com/1284664) this default behaviour will change.
  // It will be similar to the current `BuildServicesForRegularProfile()`.
  // - No services in OTR.
  // - Regular profile returns itself (original).
  // - No services for Guest and System profile.
  static ProfileSelections BuildDefault();

  // No services for all profiles.
  static ProfileSelections BuildNoServicesForAllProfiles();

  // Only build services for the regular profile.
  static ProfileSelections BuildServicesForRegularProfile();

  // Redirects building services for incognito profile to regular Profile.
  static ProfileSelections BuildServicesRedirectedInIncognito();

  // Redirects building services for both OTR and Original profile to Original
  // Profile for all profile types (Regular, Guest and System).
  static ProfileSelections BuildServicesRedirectedInOTR();

  // Builder to construct the `ProfileSelections` parameters.
  class Builder {
   public:
    Builder();
    ~Builder();

    // Builder setters
    Builder& WithRegular(ProfileSelection selection);
    Builder& WithGuest(ProfileSelection selection);
    Builder& WithSystem(ProfileSelection selection);

    // Builds the `ProfileSelections`.
    ProfileSelections Build();

   private:
    std::unique_ptr<ProfileSelections> selections_;
  };

  // Returns the ProfileSelection based on the profile information through the
  // set mapping.
  ProfileSelection GetProfileSelection(Profile* profile) const;

 private:
  // Default constructor settings sets Regular Profile ->
  // `ProfileSelection::kOriginalOnly`. It should be constructed through the
  // Builder. Value for Guest and System profile not being overridden will
  // default to the behaviour of Regular Profile.
  ProfileSelections();

  void SetProfileSelectionForRegular(ProfileSelection selection);
  void SetProfileSelectionForGuest(ProfileSelection selection);
  void SetProfileSelectionForSystem(ProfileSelection selection);

  // Default value for the mapping of
  // Regular Profile -> `ProfileSelection::kOriginalOnly`
  // Not assigning values for Guest and System Profiles now defaults to the
  // behavior of regular profiles This will change later on to default to kNone.
  ProfileSelection regular_profile_selection_ = ProfileSelection::kOriginalOnly;
  absl::optional<ProfileSelection> guest_profile_selection_;
  absl::optional<ProfileSelection> system_profile_selection_;
};

// An intermediate interface to create KeyedServiceFactory under chrome/ that
// provides a more restricted default creation of services for non regular
// profiles.
// Those profile choices are overridable by setting the proper combination of
// `ProfileSelection` and Profile type in the `ProfileSelections` passed in the
// constructor.
//
// - Example usage, for a factory redirecting in incognito.
//
// class MyRedirectingKeyedServiceFactory: public ProfileKeyedServiceFactory {
//  private:
//   MyRedirectingKeyedServiceFactory()
//       : ProfileKeyedServiceFactory(
//             "MyRedirectingKeyedService",
//             ProfileSelections::BuildServicesRedirectedInIncognito()) {}
//
//   KeyedService* BuildServiceInstanceFor(
//       content::BrowserContext* context) const override {
//     return new MyRedirectingKeyedService();
//   }
// };
//
//
// - Example service that does not exist in OTR (default behavior):
//
// class MyDefaultKeyedServiceFactory: public ProfileKeyedServiceFactory {
//  private:
//   MyDefaultKeyedServiceFactory()
//       : ProfileKeyedServiceFactory("MyDefaultKeyedService") {}
//
//   KeyedService* BuildServiceInstanceFor(
//       content::BrowserContext* context) const override {
//     return new MyDefaultKeyedService();
//   }
// };
class ProfileKeyedServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  ProfileKeyedServiceFactory(const ProfileKeyedServiceFactory&) = delete;
  ProfileKeyedServiceFactory& operator=(const ProfileKeyedServiceFactory&) =
      delete;

 protected:
  // Default constructor, will build the Factory with the default implementation
  // for `ProfileSelections`.
  explicit ProfileKeyedServiceFactory(const char* name);
  // Constructor taking in the overridden `ProfileSelections` for customized
  // Profile types service creation. This is the only way to override the
  // `ProfileSelections` value.
  ProfileKeyedServiceFactory(const char* name,
                             const ProfileSelections& profile_selections);
  ~ProfileKeyedServiceFactory() override;

  // BrowserContextKeyedServiceFactory
  // Final implementation of `GetBrowserContextToUse()`.
  // Selects the given context to proper context to use based on the
  // mapping in `ProfileSelections`.
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const final;

 private:
  // Value can only be set at construction.
  ProfileSelections profile_selections_;
};

#endif  // !CHROME_BROWSER_PROFILES_PROFILE_KEYED_SERVICE_FACTORY_H_
