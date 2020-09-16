// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/fake_nearby_share_certificate_storage.h"

#include <utility>

#include "chrome/browser/nearby_sharing/certificates/nearby_share_certificate_storage.h"

FakeNearbyShareCertificateStorage::Factory::Factory() = default;

FakeNearbyShareCertificateStorage::Factory::~Factory() = default;

std::unique_ptr<NearbyShareCertificateStorage>
FakeNearbyShareCertificateStorage::Factory::CreateInstance(
    PrefService* pref_service,
    leveldb_proto::ProtoDatabaseProvider* proto_database_provider,
    const base::FilePath& profile_path) {
  latest_pref_service_ = pref_service;
  latest_proto_database_provider_ = proto_database_provider;
  latest_profile_path_ = profile_path;

  auto instance = std::make_unique<FakeNearbyShareCertificateStorage>();
  instances_.push_back(instance.get());

  return instance;
}

FakeNearbyShareCertificateStorage::ReplacePublicCertificatesCall::
    ReplacePublicCertificatesCall(
        const std::vector<nearbyshare::proto::PublicCertificate>&
            public_certificates,
        ResultCallback callback)
    : public_certificates(public_certificates), callback(std::move(callback)) {}

FakeNearbyShareCertificateStorage::ReplacePublicCertificatesCall::
    ReplacePublicCertificatesCall(ReplacePublicCertificatesCall&& other) =
        default;

FakeNearbyShareCertificateStorage::ReplacePublicCertificatesCall::
    ~ReplacePublicCertificatesCall() = default;

FakeNearbyShareCertificateStorage::AddPublicCertificatesCall::
    AddPublicCertificatesCall(
        const std::vector<nearbyshare::proto::PublicCertificate>&
            public_certificates,
        ResultCallback callback)
    : public_certificates(public_certificates), callback(std::move(callback)) {}

FakeNearbyShareCertificateStorage::AddPublicCertificatesCall::
    AddPublicCertificatesCall(AddPublicCertificatesCall&& other) = default;

FakeNearbyShareCertificateStorage::AddPublicCertificatesCall::
    ~AddPublicCertificatesCall() = default;

FakeNearbyShareCertificateStorage::RemoveExpiredPublicCertificatesCall::
    RemoveExpiredPublicCertificatesCall(base::Time now, ResultCallback callback)
    : now(now), callback(std::move(callback)) {}

FakeNearbyShareCertificateStorage::RemoveExpiredPublicCertificatesCall::
    RemoveExpiredPublicCertificatesCall(
        RemoveExpiredPublicCertificatesCall&& other) = default;

FakeNearbyShareCertificateStorage::RemoveExpiredPublicCertificatesCall::
    ~RemoveExpiredPublicCertificatesCall() = default;

FakeNearbyShareCertificateStorage::FakeNearbyShareCertificateStorage() =
    default;

FakeNearbyShareCertificateStorage::~FakeNearbyShareCertificateStorage() =
    default;

std::vector<std::string>
FakeNearbyShareCertificateStorage::GetPublicCertificateIds() const {
  return public_certificate_ids_;
}

void FakeNearbyShareCertificateStorage::GetPublicCertificates(
    PublicCertificateCallback callback) {
  get_public_certificates_callbacks_.push_back(std::move(callback));
}

base::Optional<std::vector<NearbySharePrivateCertificate>>
FakeNearbyShareCertificateStorage::GetPrivateCertificates() const {
  return private_certificates_;
}

base::Optional<base::Time>
FakeNearbyShareCertificateStorage::NextPublicCertificateExpirationTime() const {
  return next_public_certificate_expiration_time_;
}

void FakeNearbyShareCertificateStorage::ReplacePrivateCertificates(
    const std::vector<NearbySharePrivateCertificate>& private_certificates) {
  private_certificates_ = private_certificates;
}

void FakeNearbyShareCertificateStorage::ReplacePublicCertificates(
    const std::vector<nearbyshare::proto::PublicCertificate>&
        public_certificates,
    ResultCallback callback) {
  replace_public_certificates_calls_.emplace_back(public_certificates,
                                                  std::move(callback));
}

void FakeNearbyShareCertificateStorage::AddPublicCertificates(
    const std::vector<nearbyshare::proto::PublicCertificate>&
        public_certificates,
    ResultCallback callback) {
  add_public_certificates_calls_.emplace_back(public_certificates,
                                              std::move(callback));
}

void FakeNearbyShareCertificateStorage::RemoveExpiredPublicCertificates(
    base::Time now,
    ResultCallback callback) {
  remove_expired_public_certificates_calls_.emplace_back(now,
                                                         std::move(callback));
}

void FakeNearbyShareCertificateStorage::ClearPublicCertificates(
    ResultCallback callback) {
  clear_public_certificates_callbacks_.push_back(std::move(callback));
}

void FakeNearbyShareCertificateStorage::SetPublicCertificateIds(
    const std::vector<std::string>& ids) {
  public_certificate_ids_ = ids;
}

void FakeNearbyShareCertificateStorage::SetNextPublicCertificateExpirationTime(
    base::Optional<base::Time> time) {
  next_public_certificate_expiration_time_ = time;
}
