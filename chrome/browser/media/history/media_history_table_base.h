// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_TABLE_BASE_H_
#define CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_TABLE_BASE_H_

#include "base/memory/ref_counted.h"
#include "base/synchronization/atomic_flag.h"
#include "sql/init_status.h"

namespace base {
class UpdateableSequencedTaskRunner;
}  // namespace base

namespace google {
namespace protobuf {
class MessageLite;
}  // namespace protobuf
}  // namespace google

namespace sql {
class Database;
class Statement;
}  // namespace sql

class GURL;

namespace media_history {

// Base class for all tables in the MediaHistoryTableBase.
class MediaHistoryTableBase
    : public base::RefCountedThreadSafe<MediaHistoryTableBase> {
 public:
  // Deletes any row with the |url| and returns a bool whether it was
  // successful.
  virtual bool DeleteURL(const GURL& url);

  void SetCancelled();

 protected:
  explicit MediaHistoryTableBase(
      scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner);
  virtual ~MediaHistoryTableBase();

  // Returns a UpdateableSequencedTaskRunner that is used to run tasks on the DB
  // sequence.
  base::UpdateableSequencedTaskRunner* GetTaskRunner();

  // DB sequence functions.
  virtual sql::InitStatus CreateTableIfNonExistent() = 0;
  sql::InitStatus Initialize(sql::Database* db);
  sql::Database* DB();
  void ResetDB();
  bool CanAccessDatabase();

  // Serializes and binds the |protobuf| to be stored in |col| of the statement.
  void BindProto(sql::Statement& s,
                 int col,
                 const google::protobuf::MessageLite& protobuf);

  // Deserializes the proto stored in |col| and stores it in |protobuf|. Returns
  // true if it was successful.
  bool GetProto(sql::Statement& s,
                int col,
                google::protobuf::MessageLite& protobuf);

 private:
  friend class base::RefCountedThreadSafe<MediaHistoryTableBase>;

  base::AtomicFlag cancelled_;

  scoped_refptr<base::UpdateableSequencedTaskRunner> db_task_runner_;
  sql::Database* db_;

  DISALLOW_COPY_AND_ASSIGN(MediaHistoryTableBase);
};

}  // namespace media_history

#endif  // CHROME_BROWSER_MEDIA_HISTORY_MEDIA_HISTORY_TABLE_BASE_H_
