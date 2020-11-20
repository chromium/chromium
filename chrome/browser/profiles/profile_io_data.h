// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
#define CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_

#include <string>

#include "base/macros.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "components/prefs/pref_member.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/resource_context.h"

// Conceptually speaking, the ProfileIOData represents data that lives on the IO
// thread that is owned by a Profile.  Profile owns ProfileIOData, but will make
// sure to delete it on the IO thread (except possibly in unit tests where there
// is no IO thread).
class ProfileIOData {
 public:
  ProfileIOData();
  ~ProfileIOData();

  static ProfileIOData* FromResourceContext(content::ResourceContext* rc);

  // Returns true if |scheme| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledProtocol(const std::string& scheme);

  // Returns true if |url| is handled in Chrome, or by default handlers in
  // net::URLRequest.
  static bool IsHandledURL(const GURL& url);

  // Called by Profile.
  content::ResourceContext* GetResourceContext() const;

  // Initializes the ProfileIOData object.
  void Init() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  std::string username_hash() const {
    return username_hash_;
  }
#endif

  void InitializeOnUIThread(Profile* profile);

  // Called when the Profile is destroyed. Triggers destruction of the
  // ProfileIOData.
  void ShutdownOnUIThread();

 protected:
  // Created on the UI thread, read on the IO thread during ProfileIOData lazy
  // initialization.
  struct ProfileParams {
    ProfileParams();
    ~ProfileParams();

#if BUILDFLAG(IS_CHROMEOS_ASH)
    std::string username_hash;
    bool user_is_affiliated = false;
#endif
  };

 private:
  class ResourceContext : public content::ResourceContext {
   public:
    explicit ResourceContext(ProfileIOData* io_data);
    ~ResourceContext() override;

   private:
    friend class ProfileIOData;

    ProfileIOData* const io_data_;
  };

  // Tracks whether or not we've been lazily initialized.
  mutable bool initialized_;

  // Data from the UI thread from the Profile, used to initialize ProfileIOData.
  // Deleted after lazy initialization.
  mutable std::unique_ptr<ProfileParams> profile_params_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  mutable std::string username_hash_;
#endif

  mutable std::unique_ptr<ResourceContext> resource_context_;

  DISALLOW_COPY_AND_ASSIGN(ProfileIOData);
};

#endif  // CHROME_BROWSER_PROFILES_PROFILE_IO_DATA_H_
