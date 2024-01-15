// Copyright 2020 The Chromium Authors
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
        const std::vector<nearby::sharing::proto::PublicCertificate>&
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
        const std::vector<nearby::sharing::proto::PublicCertificate>&
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

void FakeNearbyShareCertificateStorage::GetPublicCertificates(
    PublicCertificateCallback callback) {
  get_public_certificates_callbacks_.push_back(std::move(callback));
}

std::optional<std::vector<NearbySharePrivateCertificate>>
FakeNearbyShareCertificateStorage::GetPrivateCertificates() const {
  return private_certificates_;
}

std::optional<base::Time>
FakeNearbyShareCertificateStorage::NextPublicCertificateExpirationTime() const {
  return next_public_certificate_expiration_time_;
}

void FakeNearbyShareCertificateStorage::ReplacePrivateCertificates(
    const std::vector<NearbySharePrivateCertificate>& private_certificates) {
  private_certificates_ = private_certificates;
}

void FakeNearbyShareCertificateStorage::AddPublicCertificates(
    const std::vector<nearby::sharing::proto::PublicCertificate>&
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

void FakeNearbyShareCertificateStorage::SetPublicCertificateIds(
    const std::vector<std::string>& ids) {
  public_certificate_ids_ = ids;
}

void FakeNearbyShareCertificateStorage::SetNextPublicCertificateExpirationTime(
    std::optional<base::Time> time) {
  next_public_certificate_expiration_time_ = time;
}
