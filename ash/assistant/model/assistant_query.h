// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_H_
#define ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_H_

#include <string>

#include "base/component_export.h"
#include "chromeos/ash/services/assistant/public/cpp/assistant_service.h"

namespace ash {

// AssistantQueryType ----------------------------------------------------------

// Defines possible types of an Assistant query.
enum class AssistantQueryType {
  kNull,   // See AssistantNullQuery.
  kText,   // See AssistantTextQuery.
  kVoice,  // See AssistantVoiceQuery.
};

// AssistantQuery --------------------------------------------------------------

// Base class for an Assistant query.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantQuery {
 public:
  using AssistantQuerySource = assistant::AssistantQuerySource;

  AssistantQuery(const AssistantQuery&) = delete;
  AssistantQuery& operator=(const AssistantQuery&) = delete;

  virtual ~AssistantQuery() = default;

  // Returns the type for the query.
  AssistantQueryType type() const { return type_; }

  // Returns the input source for the query.
  AssistantQuerySource source() const { return source_; }

  // Returns true if the query is empty, false otherwise.
  virtual bool Empty() const = 0;

 protected:
  AssistantQuery(AssistantQueryType type, AssistantQuerySource source)
      : type_(type), source_(source) {}

 private:
  const AssistantQueryType type_;
  const AssistantQuerySource source_;
};

// AssistantNullQuery ----------------------------------------------------------

// An null Assistant query used to signify the absence of an Assistant query.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantNullQuery
    : public AssistantQuery {
 public:
  AssistantNullQuery()
      : AssistantQuery(AssistantQueryType::kNull,
                       AssistantQuerySource::kUnspecified) {}

  AssistantNullQuery(const AssistantNullQuery&) = delete;
  AssistantNullQuery& operator=(const AssistantNullQuery&) = delete;

  ~AssistantNullQuery() override = default;

  // AssistantQuery:
  bool Empty() const override;
};

// AssistantTextQuery ----------------------------------------------------------

// An Assistant text query.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantTextQuery
    : public AssistantQuery {
 public:
  AssistantTextQuery(const std::string& text, AssistantQuerySource source)
      : AssistantQuery(AssistantQueryType::kText, source), text_(text) {}

  AssistantTextQuery(const AssistantTextQuery&) = delete;
  AssistantTextQuery& operator=(const AssistantTextQuery&) = delete;

  ~AssistantTextQuery() override = default;

  // AssistantQuery:
  bool Empty() const override;

  // Returns the text for the query.
  const std::string& text() const { return text_; }

 private:
  const std::string text_;
};

// AssistantVoiceQuery ---------------------------------------------------------

// An Assistant voice query. At the start of a voice query, both the high and
// low confidence speech portions will be empty. As speech recognition
// continues, the low confidence portion will become non-empty. As speech
// recognition improves, both the high and low confidence portions of the query
// will be non-empty. When speech is fully recognized, only the high confidence
// portion will be populated.
class COMPONENT_EXPORT(ASSISTANT_MODEL) AssistantVoiceQuery
    : public AssistantQuery {
 public:
  AssistantVoiceQuery() : AssistantVoiceQuery(std::string(), std::string()) {}

  AssistantVoiceQuery(const std::string& high_confidence_speech,
                      const std::string& low_confidence_speech = std::string())
      : AssistantQuery(AssistantQueryType::kVoice,
                       AssistantQuerySource::kVoiceInput),
        high_confidence_speech_(high_confidence_speech),
        low_confidence_speech_(low_confidence_speech) {}

  AssistantVoiceQuery(const AssistantVoiceQuery&) = delete;
  AssistantVoiceQuery& operator=(const AssistantVoiceQuery&) = delete;

  ~AssistantVoiceQuery() override = default;

  // AssistantQuery:
  bool Empty() const override;

  // Returns speech for which we have high confidence of recognition.
  const std::string& high_confidence_speech() const {
    return high_confidence_speech_;
  }

  // Returns speech for which we have low confidence of recognition.
  const std::string& low_confidence_speech() const {
    return low_confidence_speech_;
  }

 private:
  const std::string high_confidence_speech_;
  const std::string low_confidence_speech_;
};

}  // namespace ash

#endif  // ASH_ASSISTANT_MODEL_ASSISTANT_QUERY_H_
