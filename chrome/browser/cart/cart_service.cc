// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/search/ntp_features.h"

CartService::CartService(Profile* profile)
    : profile_(profile),
      cart_db_(std::make_unique<CartDB>(profile_)),
      history_service_(HistoryServiceFactory::GetForProfile(
          profile_,
          ServiceAccessType::EXPLICIT_ACCESS)) {
  if (history_service_) {
    history_service_->AddObserver(this);
  }
}

CartService::~CartService() = default;

void CartService::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kCartModuleHidden, false);
  registry->RegisterBooleanPref(prefs::kCartModuleRemoved, false);
}

void CartService::Hide() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleHidden, true);
}

void CartService::RestoreHidden() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleHidden, false);
}

bool CartService::IsHidden() {
  return profile_->GetPrefs()->GetBoolean(prefs::kCartModuleHidden);
}

void CartService::Remove() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleRemoved, true);
}

void CartService::RestoreRemoved() {
  profile_->GetPrefs()->SetBoolean(prefs::kCartModuleRemoved, false);
}

bool CartService::IsRemoved() {
  return profile_->GetPrefs()->GetBoolean(prefs::kCartModuleRemoved);
}

void CartService::LoadCart(const std::string& domain,
                           CartDB::LoadCallback callback) {
  cart_db_->LoadCart(domain, std::move(callback));
}

void CartService::LoadAllCarts(CartDB::LoadCallback callback) {
  cart_db_->LoadAllCarts(std::move(callback));
}

void CartService::AddCart(const std::string& domain,
                          const cart_db::ChromeCartContentProto& proto) {
  cart_db_->AddCart(domain, proto,
                    base::BindOnce(&CartService::OnOperationFinished,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void CartService::DeleteCart(const std::string& domain) {
  cart_db_->DeleteCart(domain, base::BindOnce(&CartService::OnOperationFinished,
                                              weak_ptr_factory_.GetWeakPtr()));
}

void CartService::OnOperationFinished(bool success) {
  DCHECK(success) << "database operation failed.";
}

void CartService::Shutdown() {
  if (history_service_) {
    history_service_->RemoveObserver(this);
  }
}

void CartService::OnURLsDeleted(history::HistoryService* history_service,
                                const history::DeletionInfo& deletion_info) {
  // TODO(crbug.com/1157892): Add more fine-grained deletion of cart data when
  // history deletion happens.
  cart_db_->DeleteAllCarts(base::BindOnce(&CartService::OnOperationFinished,
                                          weak_ptr_factory_.GetWeakPtr()));
}

CartDB* CartService::GetDB() {
  return cart_db_.get();
}
