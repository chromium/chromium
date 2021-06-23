// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_TEST_UTIL_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_TEST_UTIL_H_

#include <string>

#include "base/macros.h"

class TemplateURLService;
class TestingProfile;

// TemplateURLServiceFactoryTestUtil initializes TemplateURLServiceFactory to
// return a valid TemplateURLService instance for the given profile.
class TemplateURLServiceFactoryTestUtil {
 public:
  explicit TemplateURLServiceFactoryTestUtil(TestingProfile* profile);
  virtual ~TemplateURLServiceFactoryTestUtil();

  // Makes sure the load was successful.
  void VerifyLoad();

  // Returns the TemplateURLService.
  TemplateURLService* model() const;

 private:
  TestingProfile* profile_;

  DISALLOW_COPY_AND_ASSIGN(TemplateURLServiceFactoryTestUtil);
};

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_SERVICE_FACTORY_TEST_UTIL_H_
