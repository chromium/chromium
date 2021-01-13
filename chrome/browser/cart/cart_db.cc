// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_db.h"

#include "chrome/browser/cart/cart_db_content.pb.h"
#include "chrome/browser/persisted_state_db/profile_proto_db_factory.h"

CartDB::CartDB(content::BrowserContext* browser_context)
    : proto_db_(
          ProfileProtoDBFactory<cart_db::ChromeCartContentProto>::GetInstance()
              ->GetForProfile(browser_context)) {}
CartDB::~CartDB() = default;
