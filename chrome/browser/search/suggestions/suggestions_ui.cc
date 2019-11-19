// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/suggestions/suggestions_ui.h"

#include <map>
#include <memory>
#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/suggestions/suggestions_service_factory.h"
#include "chrome/common/url_constants.h"
#include "components/suggestions/webui/suggestions_source.h"
#include "content/public/browser/url_data_source.h"

namespace suggestions {

namespace {

// Glues a SuggestionsSource instance to //chrome.
class SuggestionsSourceWrapper : public content::URLDataSource {
 public:
  explicit SuggestionsSourceWrapper(SuggestionsService* suggestions_service);
  ~SuggestionsSourceWrapper() override;

  // content::URLDataSource implementation.
  std::string GetSource() override;
  void StartDataRequest(
      const GURL& url,
      const content::WebContents::Getter& wc_getter,
      const content::URLDataSource::GotDataCallback& callback) override;
  std::string GetMimeType(const std::string& path) override;

 private:
  SuggestionsSource suggestions_source_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsSourceWrapper);
};

SuggestionsSourceWrapper::SuggestionsSourceWrapper(
    SuggestionsService* suggestions_service)
    : suggestions_source_(suggestions_service,
                          chrome::kChromeUISuggestionsURL) {}

SuggestionsSourceWrapper::~SuggestionsSourceWrapper() {}

std::string SuggestionsSourceWrapper::GetSource() {
  return chrome::kChromeUISuggestionsHost;
}

void SuggestionsSourceWrapper::StartDataRequest(
    const GURL& url,
    const content::WebContents::Getter& wc_getter,
    const content::URLDataSource::GotDataCallback& callback) {
  suggestions_source_.StartDataRequest(
      content::URLDataSource::URLToRequestPath(url), callback);
}

std::string SuggestionsSourceWrapper::GetMimeType(const std::string& path) {
  return "text/html";
}

}  // namespace

SuggestionsUI::SuggestionsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  content::URLDataSource::Add(
      profile, std::make_unique<SuggestionsSourceWrapper>(
                   SuggestionsServiceFactory::GetForProfile(profile)));
}

SuggestionsUI::~SuggestionsUI() {}

}  // namespace suggestions
