// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/smb_client/discovery/mdns_host_locator.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/browser/browser_task_traits.h"
#include "net/base/net_errors.h"
#include "net/dns/mdns_client.h"
#include "net/dns/public/dns_protocol.h"
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

class MDnsHostLocator::Impl {
 public:
  explicit Impl(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {
    // This object is created on the UI thread, so detach the sequence checker
    // and let it re-attach the first time we are run on the IO thread.
    DETACH_FROM_SEQUENCE(sequence_checker_);
  }

  ~Impl() { DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_); }

  void FindHosts(FindHostsCallback callback);

 private:
  // Start running the mDNS query on the IO thread.
  void FindHostsOnIOThread();

  // Makes the MDnsClient start listening on port 5353 on each network
  // interface.
  bool StartListening();

  // Creates a PTR transaction and finds all SMB services in the network.
  bool CreatePtrTransaction();

  // Creates an SRV transaction, which returns the hostname of |service|.
  void CreateSrvTransaction(const std::string& service);

  // Creates an A transaction, which returns the address of |raw_hostname|.
  void CreateATransaction(const std::string& raw_hostname);

  // Handler for the PTR transaction request. Returns true if the transaction
  // successfully starts.
  void OnPtrTransactionResponse(net::MDnsTransaction::Result result,
                                const net::RecordParsed* record);

  // Handler for the SRV transaction request.
  void OnSrvTransactionResponse(net::MDnsTransaction::Result result,
                                const net::RecordParsed* record);

  // Handler for the A transaction request.
  void OnATransactionResponse(const std::string& raw_hostname,
                              net::MDnsTransaction::Result result,
                              const net::RecordParsed* record);

  // Resolves services that were found through a PTR transaction request. If
  // there are no more services to be processed, this will call the
  // FindHostsCallback with the hosts found.
  void ResolveServicesFound();

  // Fires the callback if there are no more transactions left.
  void FireCallbackIfFinished();

  // Fires the callback immediately. If |success| is true, return with the hosts
  // that were found.
  void FireCallback(bool success);

 private:
  // IO thread task runner.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  uint32_t remaining_transactions_ = 0;
  std::vector<std::string> services_;
  HostMap results_;
  FindHostsCallback callback_;

  // Network stack mDNS client.
  std::unique_ptr<net::MDnsSocketFactory> socket_factory_;
  std::unique_ptr<net::MDnsClient> mdns_client_;
  std::vector<std::unique_ptr<net::MDnsTransaction>> transactions_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(Impl);
};

MDnsHostLocator::MDnsHostLocator()
    : io_task_runner_(
          base::CreateSingleThreadTaskRunner({content::BrowserThread::IO})),
      impl_(nullptr, base::OnTaskRunnerDeleter(io_task_runner_)) {}

MDnsHostLocator::~MDnsHostLocator() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MDnsHostLocator::FindHosts(FindHostsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Reset any existing query.
  weak_factory_.InvalidateWeakPtrs();
  impl_.reset(new Impl(io_task_runner_));

  callback_ = std::move(callback);
  impl_->FindHosts(base::BindOnce(
      &MDnsHostLocator::PostFindHostsDone, base::ThreadTaskRunnerHandle::Get(),
      base::BindOnce(&MDnsHostLocator::OnFindHostsDone,
                     weak_factory_.GetWeakPtr())));
}

// static
void MDnsHostLocator::PostFindHostsDone(
    scoped_refptr<base::TaskRunner> task_runner,
    FindHostsCallback callback,
    bool success,
    const HostMap& hosts) {
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(std::move(callback), success, hosts));
}

void MDnsHostLocator::OnFindHostsDone(bool success, const HostMap& hosts) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  weak_factory_.InvalidateWeakPtrs();
  impl_.reset();

  std::move(callback_).Run(success, hosts);
}

void MDnsHostLocator::Impl::FindHosts(FindHostsCallback callback) {
  DCHECK(callback_.is_null());
  callback_ = std::move(callback);
  task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&MDnsHostLocator::Impl::FindHostsOnIOThread,
                                base::Unretained(this)));
}

void MDnsHostLocator::Impl::FindHostsOnIOThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!(StartListening() && CreatePtrTransaction())) {
    LOG(ERROR) << "Failed to start MDnsHostLocator";

    FireCallback(false /* success */);
    return;
  }
}

bool MDnsHostLocator::Impl::StartListening() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  socket_factory_ = net::MDnsSocketFactory::CreateDefault();
  mdns_client_ = net::MDnsClient::CreateDefault();
  int result = mdns_client_->StartListening(socket_factory_.get());
  if (result != net::OK) {
    LOG(ERROR) << "Error starting mDNS client: " << net::ErrorToString(result);
  }
  return result == net::OK;
}

bool MDnsHostLocator::Impl::CreatePtrTransaction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<MDnsTransaction> transaction =
      mdns_client_->CreateTransaction(
          net::dns_protocol::kTypePTR, kSmbMDnsServiceName,
          kPtrTransactionFlags,
          base::BindRepeating(&MDnsHostLocator::Impl::OnPtrTransactionResponse,
                              base::Unretained(this)));

  if (!transaction->Start()) {
    LOG(ERROR) << "Failed to start PTR transaction";
    return false;
  }

  transactions_.push_back(std::move(transaction));
  return true;
}

void MDnsHostLocator::Impl::CreateSrvTransaction(const std::string& service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<MDnsTransaction> transaction =
      mdns_client_->CreateTransaction(
          net::dns_protocol::kTypeSRV, service, kSrvTransactionFlags,
          base::BindRepeating(&MDnsHostLocator::Impl::OnSrvTransactionResponse,
                              base::Unretained(this)));

  if (!transaction->Start()) {
    // If the transaction fails to start, fire the callback if there are no more
    // transactions left to be processed.
    LOG(ERROR) << "Failed to start SRV transaction";

    FireCallbackIfFinished();
    return;
  }

  transactions_.push_back(std::move(transaction));
}

void MDnsHostLocator::Impl::CreateATransaction(
    const std::string& raw_hostname) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  std::unique_ptr<MDnsTransaction> transaction =
      mdns_client_->CreateTransaction(
          net::dns_protocol::kTypeA, raw_hostname, kATransactionFlags,
          base::BindRepeating(&MDnsHostLocator::Impl::OnATransactionResponse,
                              base::Unretained(this), raw_hostname));

  if (!transaction->Start()) {
    // If the transaction fails to start, fire the callback if there are no more
    // transactions left to be processed.
    LOG(ERROR) << "Failed to start A transaction";

    FireCallbackIfFinished();
    return;
  }

  transactions_.push_back(std::move(transaction));
}

void MDnsHostLocator::Impl::OnPtrTransactionResponse(
    MDnsTransaction::Result result,
    const net::RecordParsed* record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

void MDnsHostLocator::Impl::OnSrvTransactionResponse(
    MDnsTransaction::Result result,
    const net::RecordParsed* record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

void MDnsHostLocator::Impl::OnATransactionResponse(
    const std::string& raw_hostname,
    MDnsTransaction::Result result,
    const net::RecordParsed* record) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

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

void MDnsHostLocator::Impl::ResolveServicesFound() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (services_.empty()) {
    // Call the callback immediately.
    FireCallback(true /* success */);
  } else {
    for (const std::string& services : services_) {
      CreateSrvTransaction(services);
    }
  }
}

void MDnsHostLocator::Impl::FireCallbackIfFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  DCHECK_GT(remaining_transactions_, 0u);
  if (--remaining_transactions_ == 0) {
    FireCallback(true /* success */);
  }
}

void MDnsHostLocator::Impl::FireCallback(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // DCHECK to ensure that remaining_transactions_ is at 0 if success is true.
  DCHECK(!success || (remaining_transactions_ == 0));

  std::move(callback_).Run(success, std::move(results_));
}

}  // namespace smb_client
}  // namespace chromeos
