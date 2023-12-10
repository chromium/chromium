// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/classroom/glanceables_classroom_types.h"

#include <optional>
#include <sstream>
#include <string>

#include "base/i18n/time_formatting.h"
#include "base/time/time.h"
#include "url/gurl.h"

namespace ash {

// ----------------------------------------------------------------------------
// GlanceablesClassroomCourse:

GlanceablesClassroomCourse::GlanceablesClassroomCourse(const std::string& id,
                                                       const std::string& name)
    : id(id), name(name) {}

std::string GlanceablesClassroomCourse::ToString() const {
  std::stringstream ss;
  ss << "id: " << id << ", name: " << name;
  return ss.str();
}

// ----------------------------------------------------------------------------
// GlanceablesClassroomAggregatedSubmissionsState
GlanceablesClassroomAggregatedSubmissionsState::
    GlanceablesClassroomAggregatedSubmissionsState(int total_count,
                                                   int number_turned_in,
                                                   int number_graded)
    : total_count(total_count),
      number_turned_in(number_turned_in),
      number_graded(number_graded) {}

void GlanceablesClassroomAggregatedSubmissionsState::Reset() {
  total_count = 0;
  number_turned_in = 0;
  number_graded = 0;
}

// ----------------------------------------------------------------------------
// GlanceablesClassroomAssignment

GlanceablesClassroomAssignment::GlanceablesClassroomAssignment(
    const std::string& course_title,
    const std::string& course_work_title,
    const GURL& link,
    const std::optional<base::Time>& due,
    const base::Time& last_update,
    std::optional<GlanceablesClassroomAggregatedSubmissionsState>
        submissions_state)
    : course_title(course_title),
      course_work_title(course_work_title),
      link(link),
      due(due),
      last_update(last_update),
      submissions_state(std::move(submissions_state)) {}

std::string GlanceablesClassroomAssignment::ToString() const {
  std::stringstream ss;
  ss << "Course Title: " << course_title
     << ", Course Work Title: " << course_work_title << ", Link: " << link;
  if (due.has_value()) {
    ss << ", Due: " << base::TimeFormatHTTP(due.value());
  }
  ss << ", Last Update: " << base::TimeFormatHTTP(last_update);
  if (submissions_state.has_value()) {
    ss << ", total_submission_count: " << submissions_state->total_count
       << ", number turned in: " << submissions_state->number_turned_in
       << ", number graded:" << submissions_state->number_graded;
  }
  return ss.str();
}

}  // namespace ash
