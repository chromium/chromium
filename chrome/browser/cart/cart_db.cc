// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_db.h"

#include "chrome/browser/persisted_state_db/session_proto_db_factory.h"
#include "components/commerce/core/proto/cart_db_content.pb.h"

CartDB::CartDB(content::BrowserContext* browser_context)
    : proto_db_(
          SessionProtoDBFactory<cart_db::ChromeCartContentProto>::GetInstance()
              ->GetForProfile(browser_context)) {}
CartDB::~CartDB() = default;

void CartDB::LoadCart(const std::string& domain, LoadCallback callback) {
  proto_db_->LoadOneEntry(domain, std::move(callback));
}

void CartDB::LoadAllCarts(LoadCallback callback) {
  proto_db_->LoadAllEntries(std::move(callback));
}

void CartDB::LoadCartsWithPrefix(const std::string& prefix,
                                 LoadCallback callback) {
  proto_db_->LoadContentWithPrefix(prefix, std::move(callback));
}

void CartDB::AddCart(const std::string& domain,
                     const cart_db::ChromeCartContentProto& proto,
                     OperationCallback callback) {
  proto_db_->InsertContent(domain, proto, std::move(callback));
}

void CartDB::DeleteCart(const std::string& domain, OperationCallback callback) {
  proto_db_->DeleteOneEntry(domain, std::move(callback));
}

void CartDB::DeleteAllCarts(OperationCallback callback) {
  proto_db_->DeleteAllContent(std::move(callback));
}

void CartDB::DeleteCartsWithPrefix(const std::string& prefix,
                                   OperationCallback callback) {
  proto_db_->DeleteContentWithPrefix(prefix, std::move(callback));
}
