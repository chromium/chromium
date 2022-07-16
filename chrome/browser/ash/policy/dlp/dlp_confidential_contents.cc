// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dlp_confidential_contents.h"

#include "chrome/browser/favicon/favicon_utils.h"
#include "content/public/browser/web_contents.h"

namespace policy {

DlpConfidentialContent::DlpConfidentialContent(
    content::WebContents* web_contents)
    : icon(favicon::TabFaviconFromWebContents(web_contents).AsImageSkia()),
      title(web_contents->GetTitle()),
      url(web_contents->GetLastCommittedURL()) {}

bool DlpConfidentialContent::operator==(
    const DlpConfidentialContent& other) const {
  return url.EqualsIgnoringRef(other.url);
}

bool DlpConfidentialContent::operator!=(
    const DlpConfidentialContent& other) const {
  return !(*this == other);
}

bool DlpConfidentialContent::operator<(
    const DlpConfidentialContent& other) const {
  return title < other.title;
}

bool DlpConfidentialContent::operator<=(
    const DlpConfidentialContent& other) const {
  return *this == other || *this < other;
}

bool DlpConfidentialContent::operator>(
    const DlpConfidentialContent& other) const {
  return !(*this <= other);
}

bool DlpConfidentialContent::operator>=(
    const DlpConfidentialContent& other) const {
  return !(*this < other);
}

DlpConfidentialContents::DlpConfidentialContents() = default;

DlpConfidentialContents::DlpConfidentialContents(
    const DlpConfidentialContents& other) = default;

DlpConfidentialContents& DlpConfidentialContents::operator=(
    const DlpConfidentialContents& other) = default;

DlpConfidentialContents::~DlpConfidentialContents() = default;

const base::flat_set<DlpConfidentialContent>&
DlpConfidentialContents::GetContents() const {
  return contents_;
}

void DlpConfidentialContents::Add(content::WebContents* web_contents) {
  contents_.insert(DlpConfidentialContent(web_contents));
}

void DlpConfidentialContents::ClearAndAdd(content::WebContents* web_contents) {
  contents_.clear();
  Add(web_contents);
}

bool DlpConfidentialContents::IsEmpty() const {
  return contents_.empty();
}

void DlpConfidentialContents::UnionWith(const DlpConfidentialContents& other) {
  contents_.insert(other.contents_.begin(), other.contents_.end());
}

void DlpConfidentialContents::DifferenceWith(
    const DlpConfidentialContents& other) {
  base::EraseIf(contents_, [&other](const DlpConfidentialContent& content) {
    return other.contents_.contains(content);
  });
}

}  // namespace policy
