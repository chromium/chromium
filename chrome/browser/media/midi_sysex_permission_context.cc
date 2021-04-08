// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/midi_sysex_permission_context.h"

#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_request_id.h"
#include "content/public/browser/child_process_security_policy.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "url/gurl.h"

MidiSysexPermissionContext::MidiSysexPermissionContext(
    content::BrowserContext* browser_context)
    : PermissionContextBase(
          browser_context,
          ContentSettingsType::MIDI_SYSEX,
          blink::mojom::PermissionsPolicyFeature::kMidiFeature) {}

MidiSysexPermissionContext::~MidiSysexPermissionContext() {}

void MidiSysexPermissionContext::UpdateTabContext(
    const permissions::PermissionRequestID& id,
    const GURL& requesting_frame,
    bool allowed) {
  content_settings::PageSpecificContentSettings* content_settings =
      content_settings::PageSpecificContentSettings::GetForFrame(
          id.render_process_id(), id.render_frame_id());
  if (!content_settings)
    return;

  if (allowed) {
    content_settings->OnContentAllowed(ContentSettingsType::MIDI_SYSEX);

    content::ChildProcessSecurityPolicy::GetInstance()
        ->GrantSendMidiSysExMessage(id.render_process_id());
  } else {
    content_settings->OnContentBlocked(ContentSettingsType::MIDI_SYSEX);
  }
}

bool MidiSysexPermissionContext::IsRestrictedToSecureOrigins() const {
  return true;
}
