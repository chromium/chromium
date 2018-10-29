// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/mdns_host_locator.h"

#include <utility>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "net/dns/dns_protocol.h"
#include "net/dns/mdns_client.h"
#include "net/dns/record_rdata.h"

namespace chromeos {
namespace smb_client {

namespace {

using net::MDnsTransaction;

constexpr char kSmbMDnsServiceName[] = "_smb._tcp.local";
constexpr char kMdnsLocalString[] = ".local";

constexpr int32_t kPtrTransactionFlags = MDnsTransaction::QUERY_NETWORK;
constexpr int32_t kSrvTransactionFlags = MDnsTransaction::SINGLE_RESULT |
                                         MDnsTransaction::QUERY_CACHE |
                                         MDnsTransaction::QUERY_NETWORK;
constexpr int32_t kATransactionFlags =
    MDnsTransaction::SINGLE_RESULT | MDnsTransaction::QUERY_CACHE;

}  // namespace

Hostname RemoveLocal(const std::string& raw_hostname) {
  if (!base::EndsWith(raw_hostname, kMdnsLocalString,
                      base::CompareCase::INSENSITIVE_ASCII)) {
    return raw_hostname;
  }

  DCHECK_GE(raw_hostname.size(), strlen(kMdnsLocalString));
  size_t ending_pos = raw_hostname.size() - strlen(kMdnsLocalString);
  return raw_hostname.substr(0, ending_pos);
}

MDnsHostLocator::MDnsHostLocator() = default;
MDnsHostLocator::~MDnsHostLocator() = default;

bool MDnsHostLocator::StartListening() {
  DCHECK(!running_);

  running_ = true;
  socket_factory_ = net::MDnsSocketFactory::CreateDefault();
  mdns_client_ = net::MDnsClient::CreateDefault();
  return mdns_client_->StartListening(socket_factory_.get());
}

void MDnsHostLocator::FindHosts(FindHostsCallback callback) {
  if (running_) {
    Reset();
  }

  if (!(StartListening() && CreatePtrTransaction())) {
    LOG(ERROR) << "Failed to start MDnsHostLocator";

    FireCallback(false /* success */);
    return;
  }

  callback_ = std::move(callback);
}

bool MDnsHostLocator::CreatePtrTransaction() {
  std::unique_ptr<MDnsTransaction> transaction =
      mdns_client_->CreateTransaction(net::dns_protocol::kTypePTR,
                                      kSmbMDnsServiceName, kPtrTransactionFlags,
                                      GetPtrTransactionHandler());

  if (!transaction->Start()) {
    LOG(ERROR) << "Failed to start PTR transaction";
    return false;
  }

  transactions_.push_back(std::move(transaction));
  return true;
}

void MDnsHostLocator::CreateSrvTransaction(const std::string& service) {
  std::unique_ptr<MDnsTransaction> transaction =
      mdns_client_->CreateTransaction(net::dns_protocol::kTypeSRV, service,
                                      kSrvTransactionFlags,
                                      GetSrvTransactionHandler());

  if (!transaction->Start()) {
    // If the transaction fails to start, fire the callback if there are no more
    // transactions left to be processed.
    LOG(ERROR) << "Failed to start SRV transaction";

    FireCallbackIfFinished();
    return;
  }

  transactions_.push_back(std::move(transaction));
}

void MDnsHostLocator::CreateATransaction(const std::string& raw_hostname) {
  std::unique_ptr<MDnsTransaction> transaction =
      mdns_client_->CreateTransaction(net::dns_protocol::kTypeA, raw_hostname,
                                      kATransactionFlags,
                                      GetATransactionHandler(raw_hostname));

  if (!transaction->Start()) {
    // If the transaction fails to start, fire the callback if there are no more
    // transactions left to be processed.
    LOG(ERROR) << "Failed to start A transaction";

    FireCallbackIfFinished();
    return;
  }

  transactions_.push_back(std::move(transaction));
}

void MDnsHostLocator::OnPtrTransactionResponse(
    MDnsTransaction::Result result,
    const net::RecordParsed* record) {
  if (result == MDnsTransaction::Result::RESULT_RECORD) {
    DCHECK(record);

    const net::PtrRecordRdata* data = record->rdata<net::PtrRecordRdata>();
    DCHECK(data);

    services_.push_back(data->ptrdomain());
  } else if (result == MDnsTransaction::Result::RESULT_DONE) {
    remaining_transactions_ = services_.size();

    ResolveServicesFound();
  } else {
    LOG(ERROR) << "Error getting a PTR transaction response";
    FireCallback(false /* success */);
  }
}

void MDnsHostLocator::OnSrvTransactionResponse(
    MDnsTransaction::Result result,
    const net::RecordParsed* record) {
  if (result != MDnsTransaction::Result::RESULT_RECORD) {
    // SRV transaction wasn't able to get a hostname. Fire the callback if there
    // are no more transactions.
    FireCallbackIfFinished();
    return;
  }

  DCHECK(record);
  const net::SrvRecordRdata* srv = record->rdata<net::SrvRecordRdata>();
  DCHECK(srv);

  CreateATransaction(srv->target());
}

void MDnsHostLocator::OnATransactionResponse(const std::string& raw_hostname,
                                             MDnsTransaction::Result result,
                                             const net::RecordParsed* record) {
  if (result == MDnsTransaction::Result::RESULT_RECORD) {
    DCHECK(record);

    const net::ARecordRdata* ip = record->rdata<net::ARecordRdata>();
    DCHECK(ip);

    results_[RemoveLocal(raw_hostname)] = ip->address().ToString();
  }

  // Regardless of what the result is, check to see if the callback can be fired
  // after an A transaction returns.
  FireCallbackIfFinished();
}

void MDnsHostLocator::ResolveServicesFound() {
  if (services_.empty()) {
    // Call the callback immediately.
    FireCallback(true /* success */);
  } else {
    for (const std::string& services : services_) {
      CreateSrvTransaction(services);
    }
  }
}

void MDnsHostLocator::FireCallbackIfFinished() {
  DCHECK_GT(remaining_transactions_, 0u);

  if (--remaining_transactions_ == 0) {
    FireCallback(true /* success */);
  }
}

void MDnsHostLocator::FireCallback(bool success) {
  // DCHECK to ensure that remaining_transactions_ is at 0 if success is true.
  DCHECK(!success || (remaining_transactions_ == 0));

  std::move(callback_).Run(success, results_);
  Reset();
}

void MDnsHostLocator::Reset() {
  services_.clear();
  transactions_.clear();
  results_.clear();
  running_ = false;
}

MDnsTransaction::ResultCallback MDnsHostLocator::GetPtrTransactionHandler() {
  return base::BindRepeating(&MDnsHostLocator::OnPtrTransactionResponse,
                             AsWeakPtr());
}

MDnsTransaction::ResultCallback MDnsHostLocator::GetSrvTransactionHandler() {
  return base::BindRepeating(&MDnsHostLocator::OnSrvTransactionResponse,
                             AsWeakPtr());
}

MDnsTransaction::ResultCallback MDnsHostLocator::GetATransactionHandler(
    const std::string& raw_hostname) {
  return base::BindRepeating(&MDnsHostLocator::OnATransactionResponse,
                             AsWeakPtr(), raw_hostname);
}

}  // namespace smb_client
}  // namespace chromeos
