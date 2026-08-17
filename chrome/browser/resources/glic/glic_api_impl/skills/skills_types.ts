// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CreateSkillRequest, Skill, SkillPreview, SkillsWebClientEvent, UpdateSkillRequest} from '../../glic_api/glic_api.js';
import {defInterface, defMessage} from '../transport/messaging.js';

// Shared between host and client.

export const SkillsHostDef = defInterface({
  name: 'SkillsHost',
  methods: [
    {
      name: 'createSkill',
      request: defMessage<{
        request: CreateSkillRequest,
      }>(),
      response: defMessage<{
        modalOpened: boolean,
      }>(),
      histogram: {id: 82},
    },
    {
      name: 'updateSkill',
      request: defMessage<{
        request: UpdateSkillRequest,
      }>(),
      response: defMessage<{
        modalOpened: boolean,
      }>(),
      histogram: {id: 83},
    },
    {
      name: 'showManageSkillsUi',
      request: defMessage<void>(),
      histogram: {id: 86},
    },
    {
      name: 'showBrowseSkillsUi',
      request: defMessage<void>(),
      histogram: {id: 95},
    },
    {
      name: 'getSkill',
      request: defMessage<{
        id: string,
      }>(),
      response: defMessage<{
        skill?: Skill,
      }>(),
      histogram: {id: 84},
    },
    {
      name: 'recordSkillsWebClientEvent',
      request: defMessage<{
        event: SkillsWebClientEvent,
      }>(),
      histogram: {id: 91},
    },
  ],
});
export type SkillsHost = typeof SkillsHostDef;

export const SkillsClientDef = defInterface({
  name: 'SkillsClient',
  methods: [
    {
      name: 'notifySkillPreviewsChanged',
      request: defMessage<{
        skillPreviews: SkillPreview[],
      }>(),
    },
    {
      name: 'notifyContextualSkillPreviewsChanged',
      request: defMessage<{
        contextualSkillPreviews: SkillPreview[],
      }>(),
    },
    {
      name: 'notifySkillPreviewChanged',
      request: defMessage<{
        skillPreview: SkillPreview,
      }>(),
    },
    {
      name: 'notifySkillDeleted',
      request: defMessage<{
        skillId: string,
      }>(),
    },
    {
      name: 'notifySkillsEnabledChanged',
      request: defMessage<{
        enabled: boolean,
      }>(),
      backgroundAllowed: true,
    },
  ],
});
export type SkillsClient = typeof SkillsClientDef;
