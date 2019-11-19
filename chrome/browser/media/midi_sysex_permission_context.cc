// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_sysex_permission_context.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/permissions/permission_request_id.h"
#include "content/public/browser/child_process_security_policy.h"
#include "url/gurl.h"

MidiSysexPermissionContext::MidiSysexPermissionContext(Profile* profile)
    : PermissionContextBase(profile,
                            ContentSettingsType::MIDI_SYSEX,
                            blink::mojom::FeaturePolicyFeature::kMidiFeature) {}

MidiSysexPermissionContext::~MidiSysexPermissionContext() {}

void MidiSysexPermissionContext::UpdateTabContext(const PermissionRequestID& id,
                                                  const GURL& requesting_frame,
                                                  bool allowed) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::GetForFrame(id.render_process_id(),
                                              id.render_frame_id());
  if (!content_settings)
    return;

  if (allowed) {
    content_settings->OnMidiSysExAccessed(requesting_frame);

    content::ChildProcessSecurityPolicy::GetInstance()
        ->GrantSendMidiSysExMessage(id.render_process_id());
  } else {
    content_settings->OnMidiSysExAccessBlocked(requesting_frame);
  }
}

bool MidiSysexPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
