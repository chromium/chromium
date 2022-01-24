// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_
#define CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_

#include <string>

#include "base/containers/flat_set.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}

namespace policy {

// Keeps track of title and corresponding icon of a WebContents object.
// Used to cache and later show information about observed confidential contents
// to the user.
struct DlpConfidentialContent {
  DlpConfidentialContent() = default;
  // Constructs DlpConfidentialContent from the title and icon obtained from
  // |web_contents|, which cannot be null.
  explicit DlpConfidentialContent(content::WebContents* web_contents);
  DlpConfidentialContent(const DlpConfidentialContent& other) = default;
  DlpConfidentialContent& operator=(const DlpConfidentialContent& other) =
      default;
  ~DlpConfidentialContent() = default;

  // Contents with the same url are considered equal, ignoring the ref (part
  // after #).
  bool operator==(const DlpConfidentialContent& other) const;
  bool operator!=(const DlpConfidentialContent& other) const;
  bool operator<(const DlpConfidentialContent& other) const;
  bool operator<=(const DlpConfidentialContent& other) const;
  bool operator>(const DlpConfidentialContent& other) const;
  bool operator>=(const DlpConfidentialContent& other) const;

  gfx::ImageSkia icon;
  std::u16string title;
  GURL url;
};

// Provides basic functions for storing and working with confidential contents.
// TODO(crbug.com/1264803): Limit the size + delete if related WebContents are
// destroyed
class DlpConfidentialContents {
 public:
  DlpConfidentialContents();
  DlpConfidentialContents(const DlpConfidentialContents& other);
  DlpConfidentialContents& operator=(const DlpConfidentialContents& other);
  ~DlpConfidentialContents();

  // Returns a reference to the underlying content container.
  const base::flat_set<DlpConfidentialContent>& GetContents() const;

  // Converts |web_contents| to a DlpConfidentialContent and adds it to the
  // underlying container.
  void Add(content::WebContents* web_contents);
  // Removes all stored confidential content, if there was any, and adds
  // |web_contents| converted to a DlpConfidentialContent.
  void ClearAndAdd(content::WebContents* web_contents);

  // Returns whether there is any content stored or not.
  bool IsEmpty() const;

  // Adds all content stored in |other| to the underlying container, without
  // duplicates.
  void UnionWith(const DlpConfidentialContents& other);

  // Removes all content that also exists in |other|.
  void DifferenceWith(const DlpConfidentialContents& other);

 private:
  base::flat_set<DlpConfidentialContent> contents_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_DLP_DLP_CONFIDENTIAL_CONTENTS_H_
