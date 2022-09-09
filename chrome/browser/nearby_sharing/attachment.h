// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_ATTACHMENT_H_
#define CHROME_BROWSER_NEARBY_SHARING_ATTACHMENT_H_

#include <stdint.h>
#include <string>

#include "chrome/browser/ui/webui/nearby_share/nearby_share_share_type.mojom.h"

struct ShareTarget;

// A single attachment to be sent by / received from a ShareTarget, can be
// either a file, text, or Wi-Fi credentials.
class Attachment {
 public:
  enum class Family {
    kFile,
    kText,
    kWifiCredentials,
    kMaxValue = kWifiCredentials
  };

  Attachment(Family family, int64_t size);
  Attachment(int64_t id, Family family, int64_t size);
  Attachment(const Attachment&);
  Attachment(Attachment&&);
  Attachment& operator=(const Attachment&);
  Attachment& operator=(Attachment&&);
  virtual ~Attachment();

  int64_t id() const { return id_; }
  Family family() const { return family_; }
  int64_t size() const { return size_; }
  void set_size(int64_t size) { size_ = size; }

  virtual void MoveToShareTarget(ShareTarget& share_target) = 0;
  virtual const std::string& GetDescription() const = 0;
  virtual nearby_share::mojom::ShareType GetShareType() const = 0;

 private:
  int64_t id_;
  Family family_;
  int64_t size_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_ATTACHMENT_H_
