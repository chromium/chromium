// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/app_data_migrator.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/indexed_db_context.h"
#include "content/public/browser/storage_partition.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/common/file_system/file_system_types.h"
#include "url/origin.h"

using base::WeakPtr;
using content::BrowserContext;
using content::BrowserThread;
using content::IndexedDBContext;
using content::StoragePartition;
using storage::FileSystemContext;
using storage::SandboxFileSystemBackendDelegate;

namespace {

void MigrateOnFileSystemThread(FileSystemContext* old_fs_context,
                               FileSystemContext* fs_context,
                               const extensions::Extension* extension) {
  DCHECK(
      old_fs_context->default_file_task_runner()->RunsTasksInCurrentSequence());

  SandboxFileSystemBackendDelegate* old_sandbox_delegate =
      old_fs_context->sandbox_delegate();
  SandboxFileSystemBackendDelegate* sandbox_delegate =
      fs_context->sandbox_delegate();

  GURL extension_url =
      extensions::Extension::GetBaseURLFromExtensionId(extension->id());

  std::unique_ptr<storage::SandboxFileSystemBackendDelegate::OriginEnumerator>
      enumerator(old_sandbox_delegate->CreateOriginEnumerator());

  // Find out if there is a file system that needs migration.
  GURL origin;
  do {
    origin = enumerator->Next();
  } while (origin != extension_url && !origin.is_empty());

  if (!origin.is_empty()) {
    // Copy the temporary file system.
    if (enumerator->HasFileSystemType(storage::kFileSystemTypeTemporary)) {
      old_sandbox_delegate->CopyFileSystem(
          extension_url, storage::kFileSystemTypeTemporary, sandbox_delegate);
    }
    // Copy the persistent file system.
    if (enumerator->HasFileSystemType(storage::kFileSystemTypePersistent)) {
      old_sandbox_delegate->CopyFileSystem(
          extension_url, storage::kFileSystemTypePersistent, sandbox_delegate);
    }
  }
}

void MigrateOnIndexedDBThread(IndexedDBContext* old_indexed_db_context,
                              IndexedDBContext* indexed_db_context,
                              const extensions::Extension* extension) {
  DCHECK(old_indexed_db_context->TaskRunner()->RunsTasksInCurrentSequence());

  url::Origin extension_origin = url::Origin::Create(
      extensions::Extension::GetBaseURLFromExtensionId(extension->id()));

  old_indexed_db_context->CopyOriginData(extension_origin, indexed_db_context);
}

void MigrateFileSystem(WeakPtr<extensions::AppDataMigrator> migrator,
                       StoragePartition* old_partition,
                       StoragePartition* current_partition,
                       const extensions::Extension* extension,
                       const base::Closure& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Since this method is static and it's being run as a closure task, check to
  // make sure the calling object is still around.
  if (!migrator.get()) {
    return;
  }

  FileSystemContext* old_fs_context = old_partition->GetFileSystemContext();
  FileSystemContext* fs_context = current_partition->GetFileSystemContext();

  // Perform the file system migration on the old file system's
  // sequenced task runner. This is to ensure it queues after any
  // in-flight file system operations. After it completes, it should
  // invoke the original callback passed into DoMigrationAndReply.
  old_fs_context->default_file_task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &MigrateOnFileSystemThread, base::RetainedRef(old_fs_context),
          base::RetainedRef(fs_context), base::RetainedRef(extension)),
      reply);
}

void MigrateLegacyPartition(WeakPtr<extensions::AppDataMigrator> migrator,
                            StoragePartition* old_partition,
                            StoragePartition* current_partition,
                            const extensions::Extension* extension,
                            const base::Closure& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  IndexedDBContext* indexed_db_context =
      current_partition->GetIndexedDBContext();
  IndexedDBContext* old_indexed_db_context =
      old_partition->GetIndexedDBContext();

  // Create a closure for the file system migration. This is the next step in
  // the migration flow after the IndexedDB migration.
  base::Closure migrate_fs =
      base::Bind(&MigrateFileSystem, migrator, old_partition, current_partition,
                 base::RetainedRef(extension), reply);

  // Perform the IndexedDB migration on the old context's sequenced task
  // runner. After completion, it should call MigrateFileSystem.
  old_indexed_db_context->TaskRunner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &MigrateOnIndexedDBThread, base::RetainedRef(old_indexed_db_context),
          base::RetainedRef(indexed_db_context), base::RetainedRef(extension)),
      migrate_fs);
}

}  // namespace

namespace extensions {

AppDataMigrator::AppDataMigrator(Profile* profile, ExtensionRegistry* registry)
    : profile_(profile), registry_(registry) {}

AppDataMigrator::~AppDataMigrator() {
}

bool AppDataMigrator::NeedsMigration(const Extension* old,
                                     const Extension* extension) {
  return old && old->is_legacy_packaged_app() && extension->is_platform_app();
}

void AppDataMigrator::DoMigrationAndReply(const Extension* old,
                                          const Extension* extension,
                                          const base::Closure& reply) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(NeedsMigration(old, extension));

  // This should retrieve the general storage partition.
  content::StoragePartition* old_partition =
      BrowserContext::GetStoragePartitionForSite(
          profile_, Extension::GetBaseURLFromExtensionId(extension->id()));

  // Enable the new extension so we can access its storage partition.
  bool old_was_disabled = registry_->AddEnabled(extension);

  // This should create a new isolated partition for the new version of the
  // extension.
  StoragePartition* new_partition = BrowserContext::GetStoragePartitionForSite(
      profile_, Extension::GetBaseURLFromExtensionId(extension->id()));

  // Now, restore the enabled/disabled state of the new and old extensions.
  if (old_was_disabled)
    registry_->RemoveEnabled(extension->id());
  else
    registry_->AddEnabled(old);

  MigrateLegacyPartition(weak_factory_.GetWeakPtr(), old_partition,
                         new_partition, extension, reply);
}

}  // namespace extensions
