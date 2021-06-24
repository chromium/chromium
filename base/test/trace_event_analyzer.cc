// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/trace_event_analyzer.h"

#include <math.h>

#include <algorithm>
#include <set>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted_memory.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/pattern.h"
#include "base/trace_event/trace_buffer.h"
#include "base/trace_event/trace_config.h"
#include "base/trace_event/trace_log.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace {
void OnTraceDataCollected(base::OnceClosure quit_closure,
                          base::trace_event::TraceResultBuffer* buffer,
                          const scoped_refptr<base::RefCountedString>& json,
                          bool has_more_events) {
  buffer->AddFragment(json->data());
  if (!has_more_events)
    std::move(quit_closure).Run();
}
}  // namespace

namespace trace_analyzer {

// TraceEvent

TraceEvent::TraceEvent() : thread(0, 0) {}

TraceEvent::TraceEvent(TraceEvent&& other) = default;

TraceEvent::~TraceEvent() = default;

TraceEvent& TraceEvent::operator=(TraceEvent&& rhs) = default;

bool TraceEvent::SetFromJSON(const base::Value* event_value) {
  if (!event_value->is_dict()) {
    LOG(ERROR) << "Value must be Type::DICTIONARY";
    return false;
  }

  const std::string* maybe_phase = event_value->FindStringKey("ph");
  if (!maybe_phase) {
    LOG(ERROR) << "ph is missing from TraceEvent JSON";
    return false;
  }

  phase = *maybe_phase->data();

  bool may_have_duration = (phase == TRACE_EVENT_PHASE_COMPLETE);
  bool require_origin = (phase != TRACE_EVENT_PHASE_METADATA);
  bool require_id = (phase == TRACE_EVENT_PHASE_ASYNC_BEGIN ||
                     phase == TRACE_EVENT_PHASE_ASYNC_STEP_INTO ||
                     phase == TRACE_EVENT_PHASE_ASYNC_STEP_PAST ||
                     phase == TRACE_EVENT_PHASE_MEMORY_DUMP ||
                     phase == TRACE_EVENT_PHASE_ENTER_CONTEXT ||
                     phase == TRACE_EVENT_PHASE_LEAVE_CONTEXT ||
                     phase == TRACE_EVENT_PHASE_CREATE_OBJECT ||
                     phase == TRACE_EVENT_PHASE_DELETE_OBJECT ||
                     phase == TRACE_EVENT_PHASE_SNAPSHOT_OBJECT ||
                     phase == TRACE_EVENT_PHASE_ASYNC_END);

  if (require_origin) {
    absl::optional<int> maybe_process_id = event_value->FindIntKey("pid");
    if (!maybe_process_id) {
      LOG(ERROR) << "pid is missing from TraceEvent JSON";
      return false;
    }
    thread.process_id = *maybe_process_id;

    absl::optional<int> maybe_thread_id = event_value->FindIntKey("tid");
    if (!maybe_thread_id) {
      LOG(ERROR) << "tid is missing from TraceEvent JSON";
      return false;
    }
    thread.thread_id = *maybe_thread_id;

    absl::optional<double> maybe_timestamp = event_value->FindDoubleKey("ts");
    if (!maybe_timestamp) {
      LOG(ERROR) << "ts is missing from TraceEvent JSON";
      return false;
    }
    timestamp = *maybe_timestamp;
  }
  if (may_have_duration) {
    absl::optional<double> maybe_duration = event_value->FindDoubleKey("dur");
    if (maybe_duration)
      duration = *maybe_duration;
  }
  const std::string* maybe_category = event_value->FindStringKey("cat");
  if (!maybe_category) {
    LOG(ERROR) << "cat is missing from TraceEvent JSON";
    return false;
  }
  category = *maybe_category;
  const std::string* maybe_name = event_value->FindStringKey("name");
  if (!maybe_name) {
    LOG(ERROR) << "name is missing from TraceEvent JSON";
    return false;
  }
  name = *maybe_name;
  const base::Value* maybe_args = event_value->FindDictKey("args");
  if (!maybe_args) {
    // If argument filter is enabled, the arguments field contains a string
    // value.
    const std::string* maybe_stripped_args = event_value->FindStringKey("args");
    if (!maybe_stripped_args || *maybe_stripped_args != "__stripped__") {
      LOG(ERROR) << "args is missing from TraceEvent JSON";
      return false;
    }
  }
  const base::Value* maybe_id2 = nullptr;
  if (require_id) {
    const std::string* maybe_id = event_value->FindStringKey("id");
    maybe_id2 = event_value->FindDictKey("id2");
    if (!maybe_id && !maybe_id2) {
      LOG(ERROR)
          << "id/id2 is missing from ASYNC_BEGIN/ASYNC_END TraceEvent JSON";
      return false;
    }
    if (maybe_id)
      id = *maybe_id;
  }

  absl::optional<double> maybe_thread_duration =
      event_value->FindDoubleKey("tdur");
  if (maybe_thread_duration) {
    thread_duration = *maybe_thread_duration;
  }
  absl::optional<double> maybe_thread_timestamp =
      event_value->FindDoubleKey("tts");
  if (maybe_thread_timestamp) {
    thread_timestamp = *maybe_thread_timestamp;
  }
  const std::string* maybe_scope = event_value->FindStringKey("scope");
  if (maybe_scope) {
    scope = *maybe_scope;
  }
  const std::string* maybe_bind_id = event_value->FindStringKey("bind_id");
  if (maybe_bind_id) {
    bind_id = *maybe_bind_id;
  }
  absl::optional<bool> maybe_flow_out = event_value->FindBoolKey("flow_out");
  if (maybe_flow_out) {
    flow_out = *maybe_flow_out;
  }
  absl::optional<bool> maybe_flow_in = event_value->FindBoolKey("flow_in");
  if (maybe_flow_in) {
    flow_in = *maybe_flow_in;
  }

  if (maybe_id2) {
    const std::string* maybe_global_id2 = maybe_id2->FindStringKey("global");
    if (maybe_global_id2) {
      global_id2 = *maybe_global_id2;
    }
    const std::string* maybe_local_id2 = maybe_id2->FindStringKey("local");
    if (maybe_local_id2) {
      local_id2 = *maybe_local_id2;
    }
  }

  // For each argument, copy the type and create a trace_analyzer::TraceValue.
  if (maybe_args) {
    for (auto pair : maybe_args->DictItems()) {
      switch (pair.second.type()) {
        case base::Value::Type::STRING:
          arg_strings[pair.first] = pair.second.GetString();
          break;

        case base::Value::Type::INTEGER:
          arg_numbers[pair.first] = static_cast<double>(pair.second.GetInt());
          break;

        case base::Value::Type::BOOLEAN:
          arg_numbers[pair.first] = pair.second.GetBool() ? 1.0 : 0.0;
          break;

        case base::Value::Type::DOUBLE:
          arg_numbers[pair.first] = pair.second.GetDouble();
          break;

        default:
          break;
      }

      // Record all arguments as values.
      arg_values[pair.first] = pair.second.Clone();
    }
  }

  return true;
}

double TraceEvent::GetAbsTimeToOtherEvent() const {
  return fabs(other_event->timestamp - timestamp);
}

bool TraceEvent::GetArgAsString(const std::string& name,
                                std::string* arg) const {
  const auto it = arg_strings.find(name);
  if (it != arg_strings.end()) {
    *arg = it->second;
    return true;
  }
  return false;
}

bool TraceEvent::GetArgAsNumber(const std::string& name,
                                double* arg) const {
  const auto it = arg_numbers.find(name);
  if (it != arg_numbers.end()) {
    *arg = it->second;
    return true;
  }
  return false;
}

bool TraceEvent::GetArgAsValue(const std::string& name,
                               base::Value* arg) const {
  const auto it = arg_values.find(name);
  if (it != arg_values.end()) {
    *arg = it->second.Clone();
    return true;
  }
  return false;
}

bool TraceEvent::HasStringArg(const std::string& name) const {
  return (arg_strings.find(name) != arg_strings.end());
}

bool TraceEvent::HasNumberArg(const std::string& name) const {
  return (arg_numbers.find(name) != arg_numbers.end());
}

bool TraceEvent::HasArg(const std::string& name) const {
  return (arg_values.find(name) != arg_values.end());
}

std::string TraceEvent::GetKnownArgAsString(const std::string& name) const {
  std::string arg_string;
  bool result = GetArgAsString(name, &arg_string);
  DCHECK(result);
  return arg_string;
}

double TraceEvent::GetKnownArgAsDouble(const std::string& name) const {
  double arg_double = 0;
  bool result = GetArgAsNumber(name, &arg_double);
  DCHECK(result);
  return arg_double;
}

int TraceEvent::GetKnownArgAsInt(const std::string& name) const {
  double arg_double = 0;
  bool result = GetArgAsNumber(name, &arg_double);
  DCHECK(result);
  return static_cast<int>(arg_double);
}

bool TraceEvent::GetKnownArgAsBool(const std::string& name) const {
  double arg_double = 0;
  bool result = GetArgAsNumber(name, &arg_double);
  DCHECK(result);
  return (arg_double != 0.0);
}

base::Value TraceEvent::GetKnownArgAsValue(const std::string& name) const {
  base::Value arg_value;
  bool result = GetArgAsValue(name, &arg_value);
  DCHECK(result);
  return arg_value;
}

// QueryNode

QueryNode::QueryNode(const Query& query) : query_(query) {
}

QueryNode::~QueryNode() = default;

// Query

Query::Query(TraceEventMember member)
    : type_(QUERY_EVENT_MEMBER),
      operator_(OP_INVALID),
      member_(member),
      number_(0),
      is_pattern_(false) {
}

Query::Query(TraceEventMember member, const std::string& arg_name)
    : type_(QUERY_EVENT_MEMBER),
      operator_(OP_INVALID),
      member_(member),
      number_(0),
      string_(arg_name),
      is_pattern_(false) {
}

Query::Query(const Query& query) = default;

Query::~Query() = default;

Query Query::String(const std::string& str) {
  return Query(str);
}

Query Query::Double(double num) {
  return Query(num);
}

Query Query::Int(int32_t num) {
  return Query(static_cast<double>(num));
}

Query Query::Uint(uint32_t num) {
  return Query(static_cast<double>(num));
}

Query Query::Bool(bool boolean) {
  return Query(boolean ? 1.0 : 0.0);
}

Query Query::Phase(char phase) {
  return Query(static_cast<double>(phase));
}

Query Query::Pattern(const std::string& pattern) {
  Query query(pattern);
  query.is_pattern_ = true;
  return query;
}

bool Query::Evaluate(const TraceEvent& event) const {
  // First check for values that can convert to bool.

  // double is true if != 0:
  double bool_value = 0.0;
  bool is_bool = GetAsDouble(event, &bool_value);
  if (is_bool)
    return (bool_value != 0.0);

  // string is true if it is non-empty:
  std::string str_value;
  bool is_str = GetAsString(event, &str_value);
  if (is_str)
    return !str_value.empty();

  DCHECK_EQ(QUERY_BOOLEAN_OPERATOR, type_)
      << "Invalid query: missing boolean expression";
  DCHECK(left_.get());
  DCHECK(right_.get() || is_unary_operator());

  if (is_comparison_operator()) {
    DCHECK(left().is_value() && right().is_value())
        << "Invalid query: comparison operator used between event member and "
           "value.";
    bool compare_result = false;
    if (CompareAsDouble(event, &compare_result))
      return compare_result;
    if (CompareAsString(event, &compare_result))
      return compare_result;
    return false;
  }
  // It's a logical operator.
  switch (operator_) {
    case OP_AND:
      return left().Evaluate(event) && right().Evaluate(event);
    case OP_OR:
      return left().Evaluate(event) || right().Evaluate(event);
    case OP_NOT:
      return !left().Evaluate(event);
    default:
      NOTREACHED();
      return false;
  }
}

bool Query::CompareAsDouble(const TraceEvent& event, bool* result) const {
  double lhs, rhs;
  if (!left().GetAsDouble(event, &lhs) || !right().GetAsDouble(event, &rhs))
    return false;
  switch (operator_) {
    case OP_EQ:
      *result = (lhs == rhs);
      return true;
    case OP_NE:
      *result = (lhs != rhs);
      return true;
    case OP_LT:
      *result = (lhs < rhs);
      return true;
    case OP_LE:
      *result = (lhs <= rhs);
      return true;
    case OP_GT:
      *result = (lhs > rhs);
      return true;
    case OP_GE:
      *result = (lhs >= rhs);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool Query::CompareAsString(const TraceEvent& event, bool* result) const {
  std::string lhs, rhs;
  if (!left().GetAsString(event, &lhs) || !right().GetAsString(event, &rhs))
    return false;
  switch (operator_) {
    case OP_EQ:
      if (right().is_pattern_)
        *result = base::MatchPattern(lhs, rhs);
      else if (left().is_pattern_)
        *result = base::MatchPattern(rhs, lhs);
      else
        *result = (lhs == rhs);
      return true;
    case OP_NE:
      if (right().is_pattern_)
        *result = !base::MatchPattern(lhs, rhs);
      else if (left().is_pattern_)
        *result = !base::MatchPattern(rhs, lhs);
      else
        *result = (lhs != rhs);
      return true;
    case OP_LT:
      *result = (lhs < rhs);
      return true;
    case OP_LE:
      *result = (lhs <= rhs);
      return true;
    case OP_GT:
      *result = (lhs > rhs);
      return true;
    case OP_GE:
      *result = (lhs >= rhs);
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool Query::EvaluateArithmeticOperator(const TraceEvent& event,
                                       double* num) const {
  DCHECK_EQ(QUERY_ARITHMETIC_OPERATOR, type_);
  DCHECK(left_.get());
  DCHECK(right_.get() || is_unary_operator());

  double lhs = 0, rhs = 0;
  if (!left().GetAsDouble(event, &lhs))
    return false;
  if (!is_unary_operator() && !right().GetAsDouble(event, &rhs))
    return false;

  switch (operator_) {
    case OP_ADD:
      *num = lhs + rhs;
      return true;
    case OP_SUB:
      *num = lhs - rhs;
      return true;
    case OP_MUL:
      *num = lhs * rhs;
      return true;
    case OP_DIV:
      *num = lhs / rhs;
      return true;
    case OP_MOD:
      *num = static_cast<double>(static_cast<int64_t>(lhs) %
                                 static_cast<int64_t>(rhs));
      return true;
    case OP_NEGATE:
      *num = -lhs;
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool Query::GetAsDouble(const TraceEvent& event, double* num) const {
  switch (type_) {
    case QUERY_ARITHMETIC_OPERATOR:
      return EvaluateArithmeticOperator(event, num);
    case QUERY_EVENT_MEMBER:
      return GetMemberValueAsDouble(event, num);
    case QUERY_NUMBER:
      *num = number_;
      return true;
    default:
      return false;
  }
}

bool Query::GetAsString(const TraceEvent& event, std::string* str) const {
  switch (type_) {
    case QUERY_EVENT_MEMBER:
      return GetMemberValueAsString(event, str);
    case QUERY_STRING:
      *str = string_;
      return true;
    default:
      return false;
  }
}

const TraceEvent* Query::SelectTargetEvent(const TraceEvent* event,
                                           TraceEventMember member) {
  if (member >= OTHER_FIRST_MEMBER && member <= OTHER_LAST_MEMBER)
    return event->other_event;
  if (member >= PREV_FIRST_MEMBER && member <= PREV_LAST_MEMBER)
    return event->prev_event;
  return event;
}

bool Query::GetMemberValueAsDouble(const TraceEvent& event,
                                   double* num) const {
  DCHECK_EQ(QUERY_EVENT_MEMBER, type_);

  // This could be a request for a member of |event| or a member of |event|'s
  // associated previous or next event. Store the target event in the_event:
  const TraceEvent* the_event = SelectTargetEvent(&event, member_);

  // Request for member of associated event, but there is no associated event.
  if (!the_event)
    return false;

  switch (member_) {
    case EVENT_PID:
    case OTHER_PID:
    case PREV_PID:
      *num = static_cast<double>(the_event->thread.process_id);
      return true;
    case EVENT_TID:
    case OTHER_TID:
    case PREV_TID:
      *num = static_cast<double>(the_event->thread.thread_id);
      return true;
    case EVENT_TIME:
    case OTHER_TIME:
    case PREV_TIME:
      *num = the_event->timestamp;
      return true;
    case EVENT_DURATION:
      if (!the_event->has_other_event())
        return false;
      *num = the_event->GetAbsTimeToOtherEvent();
      return true;
    case EVENT_COMPLETE_DURATION:
      if (the_event->phase != TRACE_EVENT_PHASE_COMPLETE)
        return false;
      *num = the_event->duration;
      return true;
    case EVENT_PHASE:
    case OTHER_PHASE:
    case PREV_PHASE:
      *num = static_cast<double>(the_event->phase);
      return true;
    case EVENT_HAS_STRING_ARG:
    case OTHER_HAS_STRING_ARG:
    case PREV_HAS_STRING_ARG:
      *num = (the_event->HasStringArg(string_) ? 1.0 : 0.0);
      return true;
    case EVENT_HAS_NUMBER_ARG:
    case OTHER_HAS_NUMBER_ARG:
    case PREV_HAS_NUMBER_ARG:
      *num = (the_event->HasNumberArg(string_) ? 1.0 : 0.0);
      return true;
    case EVENT_ARG:
    case OTHER_ARG:
    case PREV_ARG: {
      // Search for the argument name and return its value if found.
      auto num_i = the_event->arg_numbers.find(string_);
      if (num_i == the_event->arg_numbers.end())
        return false;
      *num = num_i->second;
      return true;
    }
    case EVENT_HAS_OTHER:
      // return 1.0 (true) if the other event exists
      *num = event.other_event ? 1.0 : 0.0;
      return true;
    case EVENT_HAS_PREV:
      *num = event.prev_event ? 1.0 : 0.0;
      return true;
    default:
      return false;
  }
}

bool Query::GetMemberValueAsString(const TraceEvent& event,
                                   std::string* str) const {
  DCHECK_EQ(QUERY_EVENT_MEMBER, type_);

  // This could be a request for a member of |event| or a member of |event|'s
  // associated previous or next event. Store the target event in the_event:
  const TraceEvent* the_event = SelectTargetEvent(&event, member_);

  // Request for member of associated event, but there is no associated event.
  if (!the_event)
    return false;

  switch (member_) {
    case EVENT_CATEGORY:
    case OTHER_CATEGORY:
    case PREV_CATEGORY:
      *str = the_event->category;
      return true;
    case EVENT_NAME:
    case OTHER_NAME:
    case PREV_NAME:
      *str = the_event->name;
      return true;
    case EVENT_ID:
    case OTHER_ID:
    case PREV_ID:
      *str = the_event->id;
      return true;
    case EVENT_ARG:
    case OTHER_ARG:
    case PREV_ARG: {
      // Search for the argument name and return its value if found.
      auto str_i = the_event->arg_strings.find(string_);
      if (str_i == the_event->arg_strings.end())
        return false;
      *str = str_i->second;
      return true;
    }
    default:
      return false;
  }
}

Query::Query(const std::string& str)
    : type_(QUERY_STRING),
      operator_(OP_INVALID),
      member_(EVENT_INVALID),
      number_(0),
      string_(str),
      is_pattern_(false) {
}

Query::Query(double num)
    : type_(QUERY_NUMBER),
      operator_(OP_INVALID),
      member_(EVENT_INVALID),
      number_(num),
      is_pattern_(false) {
}
const Query& Query::left() const {
  return left_->query();
}

const Query& Query::right() const {
  return right_->query();
}

Query Query::operator==(const Query& rhs) const {
  return Query(*this, rhs, OP_EQ);
}

Query Query::operator!=(const Query& rhs) const {
  return Query(*this, rhs, OP_NE);
}

Query Query::operator<(const Query& rhs) const {
  return Query(*this, rhs, OP_LT);
}

Query Query::operator<=(const Query& rhs) const {
  return Query(*this, rhs, OP_LE);
}

Query Query::operator>(const Query& rhs) const {
  return Query(*this, rhs, OP_GT);
}

Query Query::operator>=(const Query& rhs) const {
  return Query(*this, rhs, OP_GE);
}

Query Query::operator&&(const Query& rhs) const {
  return Query(*this, rhs, OP_AND);
}

Query Query::operator||(const Query& rhs) const {
  return Query(*this, rhs, OP_OR);
}

Query Query::operator!() const {
  return Query(*this, OP_NOT);
}

Query Query::operator+(const Query& rhs) const {
  return Query(*this, rhs, OP_ADD);
}

Query Query::operator-(const Query& rhs) const {
  return Query(*this, rhs, OP_SUB);
}

Query Query::operator*(const Query& rhs) const {
  return Query(*this, rhs, OP_MUL);
}

Query Query::operator/(const Query& rhs) const {
  return Query(*this, rhs, OP_DIV);
}

Query Query::operator%(const Query& rhs) const {
  return Query(*this, rhs, OP_MOD);
}

Query Query::operator-() const {
  return Query(*this, OP_NEGATE);
}


Query::Query(const Query& left, const Query& right, Operator binary_op)
    : operator_(binary_op),
      left_(new QueryNode(left)),
      right_(new QueryNode(right)),
      member_(EVENT_INVALID),
      number_(0) {
  type_ = (binary_op < OP_ADD ?
           QUERY_BOOLEAN_OPERATOR : QUERY_ARITHMETIC_OPERATOR);
}

Query::Query(const Query& left, Operator unary_op)
    : operator_(unary_op),
      left_(new QueryNode(left)),
      member_(EVENT_INVALID),
      number_(0) {
  type_ = (unary_op < OP_ADD ?
           QUERY_BOOLEAN_OPERATOR : QUERY_ARITHMETIC_OPERATOR);
}

namespace {

// Search |events| for |query| and add matches to |output|.
size_t FindMatchingEvents(const std::vector<TraceEvent>& events,
                          const Query& query,
                          TraceEventVector* output,
                          bool ignore_metadata_events) {
  for (const auto& i : events) {
    if (ignore_metadata_events && i.phase == TRACE_EVENT_PHASE_METADATA)
      continue;
    if (query.Evaluate(i))
      output->push_back(&i);
  }
  return output->size();
}

bool ParseEventsFromJson(const std::string& json,
                         std::vector<TraceEvent>* output) {
  absl::optional<base::Value> root = base::JSONReader::Read(json);

  if (!root)
    return false;

  base::Value::ListView list;
  if (root->is_list()) {
    list = root->GetList();
  } else if (root->is_dict()) {
    base::Value* trace_events = root->FindListKey("traceEvents");
    if (!trace_events)
      return false;

    list = trace_events->GetList();
  } else {
    return false;
  }

  for (const auto& item : list) {
    TraceEvent event;
    if (!event.SetFromJSON(&item))
      return false;
    output->push_back(std::move(event));
  }

  return true;
}

}  // namespace

// TraceAnalyzer

TraceAnalyzer::TraceAnalyzer()
    : ignore_metadata_events_(false), allow_association_changes_(true) {}

TraceAnalyzer::~TraceAnalyzer() = default;

// static
std::unique_ptr<TraceAnalyzer> TraceAnalyzer::Create(
    const std::string& json_events) {
  std::unique_ptr<TraceAnalyzer> analyzer(new TraceAnalyzer());
  if (analyzer->SetEvents(json_events))
    return analyzer;
  return nullptr;
}

bool TraceAnalyzer::SetEvents(const std::string& json_events) {
  raw_events_.clear();
  if (!ParseEventsFromJson(json_events, &raw_events_))
    return false;
  base::ranges::stable_sort(raw_events_);
  ParseMetadata();
  return true;
}

void TraceAnalyzer::AssociateBeginEndEvents() {
  using trace_analyzer::Query;

  Query begin(Query::EventPhaseIs(TRACE_EVENT_PHASE_BEGIN));
  Query end(Query::EventPhaseIs(TRACE_EVENT_PHASE_END));
  Query match(Query::EventName() == Query::OtherName() &&
              Query::EventCategory() == Query::OtherCategory() &&
              Query::EventTid() == Query::OtherTid() &&
              Query::EventPid() == Query::OtherPid());

  AssociateEvents(begin, end, match);
}

void TraceAnalyzer::AssociateAsyncBeginEndEvents(bool match_pid) {
  using trace_analyzer::Query;

  Query begin(
      Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_BEGIN) ||
      Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_STEP_INTO) ||
      Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_STEP_PAST));
  Query end(Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_END) ||
            Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_STEP_INTO) ||
            Query::EventPhaseIs(TRACE_EVENT_PHASE_ASYNC_STEP_PAST));
  Query match(Query::EventCategory() == Query::OtherCategory() &&
              Query::EventId() == Query::OtherId());

  if (match_pid) {
    match = match && Query::EventPid() == Query::OtherPid();
  }

  AssociateEvents(begin, end, match);
}

void TraceAnalyzer::AssociateEvents(const Query& first,
                                    const Query& second,
                                    const Query& match) {
  DCHECK(allow_association_changes_)
      << "AssociateEvents not allowed after FindEvents";

  // Search for matching begin/end event pairs. When a matching end is found,
  // it is associated with the begin event.
  std::vector<TraceEvent*> begin_stack;
  for (auto& this_event : raw_events_) {
    if (second.Evaluate(this_event)) {
      // Search stack for matching begin, starting from end.
      for (int stack_index = static_cast<int>(begin_stack.size()) - 1;
           stack_index >= 0; --stack_index) {
        TraceEvent& begin_event = *begin_stack[stack_index];

        // Temporarily set other to test against the match query.
        const TraceEvent* other_backup = begin_event.other_event;
        begin_event.other_event = &this_event;
        if (match.Evaluate(begin_event)) {
          // Found a matching begin/end pair.
          // Set the associated previous event
          this_event.prev_event = &begin_event;
          // Erase the matching begin event index from the stack.
          begin_stack.erase(begin_stack.begin() + stack_index);
          break;
        }

        // Not a match, restore original other and continue.
        begin_event.other_event = other_backup;
      }
    }
    // Even if this_event is a |second| event that has matched an earlier
    // |first| event, it can still also be a |first| event and be associated
    // with a later |second| event.
    if (first.Evaluate(this_event)) {
      begin_stack.push_back(&this_event);
    }
  }
}

void TraceAnalyzer::MergeAssociatedEventArgs() {
  for (auto& i : raw_events_) {
    // Merge all associated events with the first event.
    const TraceEvent* other = i.other_event;
    // Avoid looping by keeping set of encountered TraceEvents.
    std::set<const TraceEvent*> encounters;
    encounters.insert(&i);
    while (other && encounters.find(other) == encounters.end()) {
      encounters.insert(other);
      i.arg_numbers.insert(other->arg_numbers.begin(),
                           other->arg_numbers.end());
      i.arg_strings.insert(other->arg_strings.begin(),
                           other->arg_strings.end());
      other = other->other_event;
    }
  }
}

size_t TraceAnalyzer::FindEvents(const Query& query, TraceEventVector* output) {
  allow_association_changes_ = false;
  output->clear();
  return FindMatchingEvents(
      raw_events_, query, output, ignore_metadata_events_);
}

const TraceEvent* TraceAnalyzer::FindFirstOf(const Query& query) {
  TraceEventVector output;
  if (FindEvents(query, &output) > 0)
    return output.front();
  return nullptr;
}

const TraceEvent* TraceAnalyzer::FindLastOf(const Query& query) {
  TraceEventVector output;
  if (FindEvents(query, &output) > 0)
    return output.back();
  return nullptr;
}

const std::string& TraceAnalyzer::GetThreadName(
    const TraceEvent::ProcessThreadID& thread) {
  // If thread is not found, just add and return empty string.
  return thread_names_[thread];
}

void TraceAnalyzer::ParseMetadata() {
  for (const auto& this_event : raw_events_) {
    // Check for thread name metadata.
    if (this_event.phase != TRACE_EVENT_PHASE_METADATA ||
        this_event.name != "thread_name")
      continue;
    std::map<std::string, std::string>::const_iterator string_it =
        this_event.arg_strings.find("name");
    if (string_it != this_event.arg_strings.end())
      thread_names_[this_event.thread] = string_it->second;
  }
}

// Utility functions for collecting process-local traces and creating a
// |TraceAnalyzer| from the result.

void Start(const std::string& category_filter_string) {
  DCHECK(!base::trace_event::TraceLog::GetInstance()->IsEnabled());
  base::trace_event::TraceLog::GetInstance()->SetEnabled(
      base::trace_event::TraceConfig(category_filter_string, ""),
      base::trace_event::TraceLog::RECORDING_MODE);
}

std::unique_ptr<TraceAnalyzer> Stop() {
  DCHECK(base::trace_event::TraceLog::GetInstance()->IsEnabled());
  base::trace_event::TraceLog::GetInstance()->SetDisabled();

  base::trace_event::TraceResultBuffer buffer;
  base::trace_event::TraceResultBuffer::SimpleOutput trace_output;
  buffer.SetOutputCallback(trace_output.GetCallback());
  base::RunLoop run_loop;
  buffer.Start();
  base::trace_event::TraceLog::GetInstance()->Flush(
      base::BindRepeating(&OnTraceDataCollected, run_loop.QuitClosure(),
                          base::Unretained(&buffer)));
  run_loop.Run();
  buffer.Finish();

  return TraceAnalyzer::Create(trace_output.json_output);
}

// TraceEventVector utility functions.

bool GetRateStats(const TraceEventVector& events,
                  RateStats* stats,
                  const RateStatsOptions* options) {
  DCHECK(stats);
  // Need at least 3 events to calculate rate stats.
  const size_t kMinEvents = 3;
  if (events.size() < kMinEvents) {
    LOG(ERROR) << "Not enough events: " << events.size();
    return false;
  }

  std::vector<double> deltas;
  size_t num_deltas = events.size() - 1;
  for (size_t i = 0; i < num_deltas; ++i) {
    double delta = events.at(i + 1)->timestamp - events.at(i)->timestamp;
    if (delta < 0.0) {
      LOG(ERROR) << "Events are out of order";
      return false;
    }
    deltas.push_back(delta);
  }

  base::ranges::sort(deltas);

  if (options) {
    if (options->trim_min + options->trim_max > events.size() - kMinEvents) {
      LOG(ERROR) << "Attempt to trim too many events";
      return false;
    }
    deltas.erase(deltas.begin(), deltas.begin() + options->trim_min);
    deltas.erase(deltas.end() - options->trim_max, deltas.end());
  }

  num_deltas = deltas.size();
  double delta_sum = 0.0;
  for (size_t i = 0; i < num_deltas; ++i)
    delta_sum += deltas[i];

  stats->min_us = *base::ranges::min_element(deltas);
  stats->max_us = *base::ranges::max_element(deltas);
  stats->mean_us = delta_sum / static_cast<double>(num_deltas);

  double sum_mean_offsets_squared = 0.0;
  for (size_t i = 0; i < num_deltas; ++i) {
    double offset = fabs(deltas[i] - stats->mean_us);
    sum_mean_offsets_squared += offset * offset;
  }
  stats->standard_deviation_us =
      sqrt(sum_mean_offsets_squared / static_cast<double>(num_deltas - 1));

  return true;
}

bool FindFirstOf(const TraceEventVector& events,
                 const Query& query,
                 size_t position,
                 size_t* return_index) {
  DCHECK(return_index);
  for (size_t i = position; i < events.size(); ++i) {
    if (query.Evaluate(*events[i])) {
      *return_index = i;
      return true;
    }
  }
  return false;
}

bool FindLastOf(const TraceEventVector& events,
                const Query& query,
                size_t position,
                size_t* return_index) {
  DCHECK(return_index);
  for (size_t i = std::min(position + 1, events.size()); i != 0; --i) {
    if (query.Evaluate(*events[i - 1])) {
      *return_index = i - 1;
      return true;
    }
  }
  return false;
}

bool FindClosest(const TraceEventVector& events,
                 const Query& query,
                 size_t position,
                 size_t* return_closest,
                 size_t* return_second_closest) {
  DCHECK(return_closest);
  if (events.empty() || position >= events.size())
    return false;
  size_t closest = events.size();
  size_t second_closest = events.size();
  for (size_t i = 0; i < events.size(); ++i) {
    if (!query.Evaluate(*events.at(i)))
      continue;
    if (closest == events.size()) {
      closest = i;
      continue;
    }
    if (fabs(events.at(i)->timestamp - events.at(position)->timestamp) <
        fabs(events.at(closest)->timestamp - events.at(position)->timestamp)) {
      second_closest = closest;
      closest = i;
    } else if (second_closest == events.size()) {
      second_closest = i;
    }
  }

  if (closest < events.size() &&
      (!return_second_closest || second_closest < events.size())) {
    *return_closest = closest;
    if (return_second_closest)
      *return_second_closest = second_closest;
    return true;
  }

  return false;
}

size_t CountMatches(const TraceEventVector& events,
                    const Query& query,
                    size_t begin_position,
                    size_t end_position) {
  if (begin_position >= events.size())
    return 0u;
  end_position = (end_position < events.size()) ? end_position : events.size();
  size_t count = 0u;
  for (size_t i = begin_position; i < end_position; ++i) {
    if (query.Evaluate(*events.at(i)))
      ++count;
  }
  return count;
}

}  // namespace trace_analyzer
