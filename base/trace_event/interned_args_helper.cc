// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/interned_args_helper.h"

#include "third_party/perfetto/include/perfetto/tracing/track_event_interned_data_index.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/profiling/profile_common.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/log_message.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/source_location.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/task_execution.pbzero.h"

namespace base {
namespace trace_event {

namespace {

const void* const kModuleCacheForTracingKey = &kModuleCacheForTracingKey;

class ModuleCacheForTracing : public perfetto::TrackEventTlsStateUserData {
 public:
  ModuleCacheForTracing() = default;
  ~ModuleCacheForTracing() override = default;

  base::ModuleCache& GetModuleCache() { return module_cache_; }

 private:
  base::ModuleCache module_cache_;
};

}  // namespace

//  static
void InternedSourceLocation::Add(
    perfetto::protos::pbzero::InternedData* interned_data,
    size_t iid,
    const TraceSourceLocation& location) {
  auto* msg = interned_data->add_source_locations();
  msg->set_iid(iid);
  if (location.file_name != nullptr)
    msg->set_file_name(location.file_name);
  if (location.function_name != nullptr)
    msg->set_function_name(location.function_name);
  // TODO(ssid): Add line number once it is allowed in internal proto.
  // TODO(ssid): Add program counter to the proto fields when
  // !BUILDFLAG(ENABLE_LOCATION_SOURCE).
  // TODO(http://crbug.com760702) remove file name and just pass the program
  // counter to the heap profiler macro.
  // TODO(ssid): Consider writing the program counter of the current task
  // (from the callback function pointer) instead of location that posted the
  // task.
}

// static
void InternedLogMessage::Add(
    perfetto::protos::pbzero::InternedData* interned_data,
    size_t iid,
    const std::string& log_message) {
  auto* msg = interned_data->add_log_message_body();
  msg->set_iid(iid);
  msg->set_body(log_message);
}

// static
void InternedBuildId::Add(perfetto::protos::pbzero::InternedData* interned_data,
                          size_t iid,
                          const std::string& build_id) {
  auto* msg = interned_data->add_build_ids();
  msg->set_iid(iid);
  msg->set_str(build_id);
}

// static
void InternedMappingPath::Add(
    perfetto::protos::pbzero::InternedData* interned_data,
    size_t iid,
    const std::string& mapping_path) {
  auto* msg = interned_data->add_mapping_paths();
  msg->set_iid(iid);
  msg->set_str(mapping_path);
}

// static
size_t InternedMapping::Get(perfetto::EventContext* ctx,
                            const base::ModuleCache::Module* module) {
  auto* index_for_field = GetOrCreateIndexForField(ctx->GetIncrementalState());
  size_t iid;
  if (index_for_field->index_.LookUpOrInsert(&iid, module)) {
    return iid;
  }
  InternedMapping::Add(ctx, iid, module);
  return iid;
}

// static
void InternedMapping::Add(perfetto::EventContext* ctx,
                          size_t iid,
                          const base::ModuleCache::Module* module) {
  // TODO(b/270470700): Remove TransformModuleIDToSymbolServerFormat on all
  // platforms once tools/tracing is fixed.
  const auto build_id = InternedBuildId::Get(
      ctx, base::TransformModuleIDToSymbolServerFormat(module->GetId()));
  const auto path_id =
      InternedMappingPath::Get(ctx, module->GetDebugBasename().MaybeAsASCII());

  auto* msg =
      ctx->GetIncrementalState()->serialized_interned_data->add_mappings();
  msg->set_iid(iid);
  msg->set_build_id(build_id);
  msg->add_path_string_ids(path_id);
}

// static
std::optional<size_t> InternedUnsymbolizedSourceLocation::Get(
    perfetto::EventContext* ctx,
    uintptr_t address) {
  auto* index_for_field = GetOrCreateIndexForField(ctx->GetIncrementalState());
  ModuleCacheForTracing* module_cache = static_cast<ModuleCacheForTracing*>(
      ctx->GetTlsUserData(kModuleCacheForTracingKey));
  if (!module_cache) {
    auto new_module_cache = std::make_unique<ModuleCacheForTracing>();
    module_cache = new_module_cache.get();
    ctx->SetTlsUserData(kModuleCacheForTracingKey, std::move(new_module_cache));
  }
  const base::ModuleCache::Module* module =
      module_cache->GetModuleCache().GetModuleForAddress(address);
  if (!module) {
    return std::nullopt;
  }
  size_t iid;
  if (index_for_field->index_.LookUpOrInsert(&iid, address)) {
    return iid;
  }
  const auto mapping_id = InternedMapping::Get(ctx, module);
  const uintptr_t rel_pc = address - module->GetBaseAddress();
  InternedUnsymbolizedSourceLocation::Add(
      ctx->GetIncrementalState()->serialized_interned_data.get(), iid,
      base::trace_event::UnsymbolizedSourceLocation(mapping_id, rel_pc));
  return iid;
}

// static
void InternedUnsymbolizedSourceLocation::Add(
    perfetto::protos::pbzero::InternedData* interned_data,
    size_t iid,
    const UnsymbolizedSourceLocation& location) {
  auto* msg = interned_data->add_unsymbolized_source_locations();
  msg->set_iid(iid);
  msg->set_mapping_id(location.mapping_id);
  msg->set_rel_pc(location.rel_pc);
}

}  // namespace trace_event
}  // namespace base
