// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_galleries/media_galleries_dialog_controller_mock.h"

#include "base/strings/utf_string_conversions.h"

using ::testing::_;
using ::testing::Return;

MediaGalleriesDialogControllerMock::MediaGalleriesDialogControllerMock() {
  ON_CALL(*this, GetHeader()).WillByDefault(Return(u"Title"));
  ON_CALL(*this, GetSubtext()).WillByDefault(Return(u"Desc"));
  ON_CALL(*this, GetAcceptButtonText()).WillByDefault(Return(u"OK"));
  ON_CALL(*this, GetAuxiliaryButtonText()).WillByDefault(Return(u"Button"));
  ON_CALL(*this, GetSectionEntries(_)).
      WillByDefault(Return(Entries()));
}

MediaGalleriesDialogControllerMock::~MediaGalleriesDialogControllerMock() {
}
