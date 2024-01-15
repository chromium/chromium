// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/k_anonymity_service/k_anonymity_service_storage.h"

#include "base/functional/callback_helpers.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/k_anonymity_service/proto/k_anonymity_storage.pb.h"
#include "components/sqlite_proto/key_value_data.h"
#include "components/sqlite_proto/key_value_table.h"
#include "components/sqlite_proto/proto_table_manager.h"
#include "sql/database.h"

namespace {

namespace proto {
using k_anonymity::proto::OHTTPKeyAndExpiration;
using k_anonymity::proto::TrustTokenKeyCommitmentWithExpiration;
}  // namespace proto

const int kCurrentSchemaVersion = 1;
const int kMaxOhttpKeys = 2;

const base::TimeDelta kFlushDelay = base::Seconds(1);

const char kOhttpKeyTable[] = "ohttp_keys";
const char kTrustTokenKeyCommitmentTable[] = "trust_token_key_commitments";
const char kTrustTokenKeyCommitmentKey[] = "trust_token_key_commitment";

struct OhttpKeyExpirationComparator {
  bool operator()(const proto::OHTTPKeyAndExpiration& left,
                  const proto::OHTTPKeyAndExpiration& right) {
    return left.expiration_us() < right.expiration_us();
  }
};

class KAnonymityServiceSqlStorage : public KAnonymityServiceStorage {
 public:
  explicit KAnonymityServiceSqlStorage(base::FilePath db_storage_path)
      : db_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
             base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
        db_(std::make_unique<sql::Database>(sql::DatabaseOptions{})),
        db_storage_path_(std::move(db_storage_path)),
        table_manager_(base::MakeRefCounted<sqlite_proto::ProtoTableManager>(
            db_task_runner_)),
        ohttp_key_table_(
            std::make_unique<
                sqlite_proto::KeyValueTable<proto::OHTTPKeyAndExpiration>>(
                kOhttpKeyTable)),
        ohttp_key_data_(
            std::make_unique<
                sqlite_proto::KeyValueData<proto::OHTTPKeyAndExpiration,
                                           OhttpKeyExpirationComparator>>(
                table_manager_,
                ohttp_key_table_.get(),
                /*max_num_entries=*/kMaxOhttpKeys,
                kFlushDelay)),
        trust_token_key_commitment_table_(
            std::make_unique<sqlite_proto::KeyValueTable<
                proto::TrustTokenKeyCommitmentWithExpiration>>(
                kTrustTokenKeyCommitmentTable)),
        trust_token_key_commitment_data_(
            std::make_unique<sqlite_proto::KeyValueData<
                proto::TrustTokenKeyCommitmentWithExpiration>>(
                table_manager_,
                trust_token_key_commitment_table_.get(),
                /*max_num_entries=*/std::nullopt,
                kFlushDelay)),
        weak_factory_(this) {}

  ~KAnonymityServiceSqlStorage() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (status_ == kInitOk) {
      // Enqueue a flush
      ohttp_key_data_->FlushDataToDisk();
      trust_token_key_commitment_data_->FlushDataToDisk();
    }

    // Shutdown `table_manager_`, delete database on db
    // sequence, then delete the KeyValueTable/KeyValueData on main sequence.
    // This ensures that the flush occurs before we delete the
    // KeyValueTable/KeyValueData. We need to delay the destruction of the
    // tables until after the task on the `db_task_runner_` completes so that
    // tasks on the `db_task_runner_` don't use them after they are destroyed.
    // Note that the tables hold strong references to the `table_manager_` so it
    // will actually be destroyed during the "reply" closure.
    db_task_runner_->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<sqlite_proto::ProtoTableManager> table_manager,
               std::unique_ptr<sql::Database> db) {
              table_manager->WillShutdown();
            },
            std::move(table_manager_), std::move(db_)),
        base::DoNothingWithBoundArgs(
            std::move(ohttp_key_table_), std::move(ohttp_key_data_),
            std::move(trust_token_key_commitment_table_),
            std::move(trust_token_key_commitment_data_)));
  }

  void WaitUntilReady(base::OnceCallback<void(InitStatus)> on_ready) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    if (status_ != kInitNotReady) {
      std::move(on_ready).Run(InitStatus::kInitOk);
      return;
    }

    waiting_tasks_.emplace_back(std::move(on_ready));
    if (waiting_tasks_.size() > 1) {
      // We're in the process of initializing.
      return;
    }

    // This is safe because tasks are serialized on the db_task_runner sequence
    // and the `table_manager_`, `ohttp_key_data_`, and
    // `trust_token_key_commitment_data_` are only freed after a response from a
    // task (triggered by the destructor) run on the `db_task_runner_`.
    // Similarly, the `db_` is not actually destroyed until until the task
    // triggered by the destructor runs on the `db_task_runner_`.
    db_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            &KAnonymityServiceSqlStorage::InitializeOnDbSequence,
            base::Unretained(db_.get()), db_storage_path_,
            base::Unretained(table_manager_.get()),
            base::Unretained(ohttp_key_data_.get()),
            base::Unretained(trust_token_key_commitment_data_.get())),
        base::BindOnce(&KAnonymityServiceSqlStorage::OnReady,
                       weak_factory_.GetWeakPtr()));
  }

  void OnReady(InitStatus status) {
    status_ = status;
    for (auto& waiting_task : waiting_tasks_) {
      std::move(waiting_task).Run(status);
    }
  }

  static InitStatus InitializeOnDbSequence(
      sql::Database* db,
      base::FilePath db_storage_path,
      sqlite_proto::ProtoTableManager* table_manager,
      sqlite_proto::KeyValueData<proto::OHTTPKeyAndExpiration,
                                 OhttpKeyExpirationComparator>* ohttp_key_data,
      sqlite_proto::KeyValueData<proto::TrustTokenKeyCommitmentWithExpiration>*
          trust_token_key_commitment_data) {
    if (db->Open(db_storage_path) == false) {
      return InitStatus::kInitError;
    }
    table_manager->InitializeOnDbSequence(
        db,
        std::vector<std::string>{kOhttpKeyTable, kTrustTokenKeyCommitmentTable},
        kCurrentSchemaVersion);
    ohttp_key_data->InitializeOnDBSequence();
    trust_token_key_commitment_data->InitializeOnDBSequence();
    return InitStatus::kInitOk;
  }

  std::optional<OHTTPKeyAndExpiration> GetOHTTPKeyFor(
      const url::Origin& origin) const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(status_ != kInitNotReady);
    proto::OHTTPKeyAndExpiration result;
    if (!ohttp_key_data_->TryGetData(origin.Serialize(), &result)) {
      return std::nullopt;
    }
    return OHTTPKeyAndExpiration{
        result.hpke_key(), base::Time::FromDeltaSinceWindowsEpoch(
                               base::Microseconds(result.expiration_us()))};
  }
  void UpdateOHTTPKeyFor(const url::Origin& origin,
                         const OHTTPKeyAndExpiration& value) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(status_ != kInitNotReady);
    proto::OHTTPKeyAndExpiration proto_value;
    proto_value.set_hpke_key(value.key);
    proto_value.set_expiration_us(
        value.expiration.ToDeltaSinceWindowsEpoch().InMicroseconds());

    ohttp_key_data_->UpdateData(origin.Serialize(), std::move(proto_value));
  }

  std::optional<KeyAndNonUniqueUserIdWithExpiration> GetKeyAndNonUniqueUserId()
      const override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(status_ != kInitNotReady);
    proto::TrustTokenKeyCommitmentWithExpiration result;
    if (!trust_token_key_commitment_data_->TryGetData(
            kTrustTokenKeyCommitmentKey, &result)) {
      return std::nullopt;
    }
    return KeyAndNonUniqueUserIdWithExpiration{
        {result.key_commitment(), result.non_unique_id()},
        base::Time::FromDeltaSinceWindowsEpoch(
            base::Microseconds(result.expiration_us()))};
  }
  void UpdateKeyAndNonUniqueUserId(
      const KeyAndNonUniqueUserIdWithExpiration& value) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(status_ != kInitNotReady);
    proto::TrustTokenKeyCommitmentWithExpiration proto_value;
    proto_value.set_key_commitment(value.key_and_id.key_commitment);
    proto_value.set_non_unique_id(value.key_and_id.non_unique_user_id);
    proto_value.set_expiration_us(
        value.expiration.ToDeltaSinceWindowsEpoch().InMicroseconds());

    trust_token_key_commitment_data_->UpdateData(kTrustTokenKeyCommitmentKey,
                                                 std::move(proto_value));
  }

 private:
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
  std::unique_ptr<sql::Database> db_;
  base::FilePath db_storage_path_;

  scoped_refptr<sqlite_proto::ProtoTableManager> table_manager_;
  std::unique_ptr<sqlite_proto::KeyValueTable<proto::OHTTPKeyAndExpiration>>
      ohttp_key_table_;
  std::unique_ptr<sqlite_proto::KeyValueData<proto::OHTTPKeyAndExpiration,
                                             OhttpKeyExpirationComparator>>
      ohttp_key_data_;

  std::unique_ptr<
      sqlite_proto::KeyValueTable<proto::TrustTokenKeyCommitmentWithExpiration>>
      trust_token_key_commitment_table_;
  std::unique_ptr<
      sqlite_proto::KeyValueData<proto::TrustTokenKeyCommitmentWithExpiration>>
      trust_token_key_commitment_data_;

  InitStatus status_ = kInitNotReady;
  std::vector<base::OnceCallback<void(InitStatus)>> waiting_tasks_;
  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<KAnonymityServiceSqlStorage> weak_factory_;
};

}  // namespace

KAnonymityServiceStorage::~KAnonymityServiceStorage() = default;

KAnonymityServiceMemoryStorage::KAnonymityServiceMemoryStorage() = default;

KAnonymityServiceMemoryStorage::~KAnonymityServiceMemoryStorage() = default;

void KAnonymityServiceMemoryStorage::WaitUntilReady(
    base::OnceCallback<void(InitStatus)> on_ready) {
  std::move(on_ready).Run(InitStatus::kInitOk);
}

std::optional<OHTTPKeyAndExpiration>
KAnonymityServiceMemoryStorage::GetOHTTPKeyFor(
    const url::Origin& origin) const {
  auto it = ohttp_key_map_.find(origin);
  if (it == ohttp_key_map_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void KAnonymityServiceMemoryStorage::UpdateOHTTPKeyFor(
    const url::Origin& origin,
    const OHTTPKeyAndExpiration& key) {
  ohttp_key_map_[origin] = key;
}

std::optional<KeyAndNonUniqueUserIdWithExpiration>
KAnonymityServiceMemoryStorage::GetKeyAndNonUniqueUserId() const {
  return key_and_non_unique_user_id_with_expiration_;
}
void KAnonymityServiceMemoryStorage::UpdateKeyAndNonUniqueUserId(
    const KeyAndNonUniqueUserIdWithExpiration& key) {
  key_and_non_unique_user_id_with_expiration_ = key;
}

std::unique_ptr<KAnonymityServiceStorage> CreateKAnonymitySqlStorageForPath(
    base::FilePath db_storage_path) {
  return std::make_unique<KAnonymityServiceSqlStorage>(
      std::move(db_storage_path));
}
