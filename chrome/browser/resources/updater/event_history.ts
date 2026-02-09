// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Types and parsing functions for updater event history.
 *
 * Event history is logged by the updater as a series of structured events
 * formatted as JSON blobs. This file provides tools to parse these events and
 * organize them into a more useful structure.
 *
 * See //docs/updater/history_log.md for details on the serialization format.
 */

import {loadTimeData} from './i18n_setup.js';

// ---------------------------------------------------------------------------
// Common Types
// ---------------------------------------------------------------------------

/**
 * A list of event types which are understood by this library.
 */
export const EVENT_TYPES = [
  'INSTALL',
  'UNINSTALL',
  'QUALIFY',
  'ACTIVATE',
  'PERSISTED_DATA',
  'POST_REQUEST',
  'LOAD_POLICY',
  'UPDATE',
  'UPDATER_PROCESS',
  'APP_COMMAND',
] as const;
/**
 * The event types which are understood by this library.
 */
export type EventType = (typeof EVENT_TYPES)[number];

const BOUNDS = ['START', 'END', 'INSTANT'] as const;
/**
 * The bound of an event, indicating whether it is instantaneous ('INSTANT') or
 * part of a pair of 'START' and 'END' events.
 */
export type Bound = (typeof BOUNDS)[number];

const UPDATE_PRIORITIES = ['BACKGROUND', 'FOREGROUND'] as const;
/**
 * The priority of an update event.
 */
export type UpdatePriority = (typeof UPDATE_PRIORITIES)[number];

export const SCOPES = ['USER', 'SYSTEM'] as const;
/**
 * The scope of an updater process.
 */
export type Scope = (typeof SCOPES)[number];

/**
 * Represents an error recorded as a part of an event.
 */
export interface UpdaterError {
  category: number;
  code: number;
  extracode1: number;
}

/**
 * Common properties shared by all events.
 */
interface BaseEvent {
  eventType: EventType;
  eventId: string;
  deviceUptime: number;
  pid: number;
  processToken: string;
  bound: Bound;
  errors: UpdaterError[];
}

type StartEvent = BaseEvent&{bound: 'START'};
type EndEvent = BaseEvent&{bound: 'END'};

/**
 * Represents a pair of START and END events of the same type.
 */
// clang-format off
interface EventPair<Event,
                    Start extends StartEvent&{eventType: Event},
                    End extends EndEvent&{eventType: Event}> {
  /**
   * The type of the events in the pair.
   */
  eventType: Event;
  /**
   * The START event.
   */
  startEvent: Start;
  /**
   * The END event.
   */
  endEvent: End;
}
// clang-format on

/**
 * Represents an app registered with the updater.
 */
export interface RegisteredApp {
  /**
   * The unique ID of the app.
   */
  appId: string;
  /**
   * The version of the app.
   */
  version: string;
  cohort?: string;
  brandCode?: string;
}

/**
 * Represents the set of policies loaded by the updater.
 */
export interface PolicySet {
  policiesByName: {[key: string]: PolicyData};
  policiesByAppId: {[key: string]: {[key: string]: PolicyData}};
}

/**
 * Represents a single policy and its values by source.
 */
export interface PolicyData {
  valuesBySource: {[key: string]: unknown};
  prevailingSource: string;
}

/**
 * Returns the localized title of an event type.
 */
export function localizeEventType(eventType: EventType) {
  return loadTimeData.getString(`eventType${eventType}`);
}

export const COMMON_UPDATE_OUTCOMES =
    ['UPDATED', 'NO_UPDATE', 'UPDATE_ERROR'] as const;
export type CommonUpdateOutcome = (typeof COMMON_UPDATE_OUTCOMES)[number];

/**
 * Returns the localized title of a common update outcome.
 */
export function localizeUpdateOutcome(outcome: CommonUpdateOutcome) {
  return loadTimeData.getString(`updateOutcome${outcome}`);
}

/**
 * Returns the localized title of a scope.
 */
export function localizeScope(scope: Scope): string {
  return loadTimeData.getString(
      scope === 'SYSTEM' ? 'scopeSystem' : 'scopeUser');
}

// ---------------------------------------------------------------------------
// Specific Event Definitions
// ---------------------------------------------------------------------------

/**
 * An event marking the start of an app install.
 */
export interface InstallStartEvent extends StartEvent {
  eventType: 'INSTALL';
  appId: string;
}

/**
 * An event marking the end of an app install.
 */
export interface InstallEndEvent extends EndEvent {
  eventType: 'INSTALL';
  version?: string;
}

/**
 * A merged event representing an app install.
 */
export type MergedInstallEvent =
    EventPair<'INSTALL', InstallStartEvent, InstallEndEvent>;

/**
 * An event marking the start of an app uninstall.
 */
export interface UninstallStartEvent extends StartEvent {
  eventType: 'UNINSTALL';
  appId: string;
  version: string;
  reason: string;
}

/**
 * An event marking the end of an app uninstall.
 */
export interface UninstallEndEvent extends EndEvent {
  eventType: 'UNINSTALL';
}

/**
 * A merged event representing an app uninstall.
 */
export type MergedUninstallEvent =
    EventPair<'UNINSTALL', UninstallStartEvent, UninstallEndEvent>;

/**
 * An event marking the start of updater qualification.
 */
export interface QualifyStartEvent extends StartEvent {
  eventType: 'QUALIFY';
}

/**
 * An event marking the end of updater qualification.
 */
export interface QualifyEndEvent extends EndEvent {
  eventType: 'QUALIFY';
  qualified?: boolean;
}

/**
 * A merged event representing an updater qualification.
 */
export type MergedQualifyEvent =
    EventPair<'QUALIFY', QualifyStartEvent, QualifyEndEvent>;

/**
 * An event marking the start of updater activation.
 */
export interface ActivateStartEvent extends StartEvent {
  eventType: 'ACTIVATE';
}

/**
 * An event marking the end of updater activation.
 */
export interface ActivateEndEvent extends EndEvent {
  eventType: 'ACTIVATE';
  activated?: boolean;
}

/**
 * A merged event representing an updater activation.
 */
export type MergedActivateEvent =
    EventPair<'ACTIVATE', ActivateStartEvent, ActivateEndEvent>;

/**
 * An event marking the start of an Omaha request.
 */
export interface PostRequestStartEvent extends StartEvent {
  eventType: 'POST_REQUEST';
  request: string;
}

/**
 * An event marking the end of an Omaha request.
 */
export interface PostRequestEndEvent extends EndEvent {
  eventType: 'POST_REQUEST';
  response?: string;
}

/**
 * A merged event representing an Omaha request.
 */
export type MergedPostRequestEvent =
    EventPair<'POST_REQUEST', PostRequestStartEvent, PostRequestEndEvent>;

/**
 * An event marking the start of policy loading.
 */
export interface LoadPolicyStartEvent extends StartEvent {
  eventType: 'LOAD_POLICY';
}

/**
 * An event marking the end of policy loading.
 */
export interface LoadPolicyEndEvent extends EndEvent {
  eventType: 'LOAD_POLICY';
  policySet?: PolicySet;
}

/**
 * A merged event representing a policy load.
 */
export type MergedLoadPolicyEvent =
    EventPair<'LOAD_POLICY', LoadPolicyStartEvent, LoadPolicyEndEvent>;

/**
 * An event marking the start of an update operation.
 */
export interface UpdateStartEvent extends StartEvent {
  eventType: 'UPDATE';
  appId?: string;
  priority?: UpdatePriority;
}

/**
 * An event marking the end of an update operation.
 */
export interface UpdateEndEvent extends EndEvent {
  eventType: 'UPDATE';
  outcome?: string;
  nextVersion?: string;
}

/**
 * A merged event representing a update operation.
 */
export type MergedUpdateEvent =
    EventPair<'UPDATE', UpdateStartEvent, UpdateEndEvent>;

/**
 * An event marking the start of an updater process.
 */
export interface UpdaterProcessStartEvent extends StartEvent {
  eventType: 'UPDATER_PROCESS';
  commandLine?: string;
  timestamp?: Date;
  updaterVersion?: string;
  scope?: Scope;
  osPlatform?: string;
  osVersion?: string;
  osArchitecture?: string;
  updaterArchitecture?: string;
  parentPid?: number;
}

/**
 * An event marking the end of an updater process.
 */
export interface UpdaterProcessEndEvent extends EndEvent {
  eventType: 'UPDATER_PROCESS';
  exitCode?: number;
}

/**
 * A merged event representing an updater process.
 */
export type MergedUpdaterProcessEvent = EventPair<
    'UPDATER_PROCESS', UpdaterProcessStartEvent, UpdaterProcessEndEvent>;

/**
 * An event marking the start of an app command.
 */
export interface AppCommandStartEvent extends StartEvent {
  eventType: 'APP_COMMAND';
  appId: string;
  commandLine?: string;
}

/**
 * An event marking the end of an app command.
 */
export interface AppCommandEndEvent extends EndEvent {
  eventType: 'APP_COMMAND';
  exitCode?: number;
  output?: string;
}

/**
 * A merged event representing an app command.
 */
export type MergedAppCommandEvent =
    EventPair<'APP_COMMAND', AppCommandStartEvent, AppCommandEndEvent>;

/**
 * An event containing persisted data from the updater.
 */
export interface PersistedDataEvent extends BaseEvent {
  eventType: 'PERSISTED_DATA';
  bound: 'INSTANT';
  eulaRequired: boolean;
  lastChecked?: Date;
  lastStarted?: Date;
  registeredApps: RegisteredApp[];
}

/**
 * Represents any event understood by this library that can be present in event
 * history.
 */
export type HistoryEvent =
    |InstallStartEvent|InstallEndEvent|UninstallStartEvent|UninstallEndEvent|
    QualifyStartEvent|QualifyEndEvent|ActivateStartEvent|ActivateEndEvent|
    PostRequestStartEvent|PostRequestEndEvent|LoadPolicyStartEvent|
    LoadPolicyEndEvent|UpdateStartEvent|UpdateEndEvent|UpdaterProcessStartEvent|
    UpdaterProcessEndEvent|AppCommandStartEvent|AppCommandEndEvent|
    PersistedDataEvent;

/**
 * Represents any merged event pair understood by this library that can be
 * present in event history.
 */
export type MergedHistoryEvent =
    |MergedInstallEvent|MergedUninstallEvent|MergedQualifyEvent|
    MergedActivateEvent|MergedPostRequestEvent|MergedLoadPolicyEvent|
    MergedUpdateEvent|MergedUpdaterProcessEvent|MergedAppCommandEvent;

// ---------------------------------------------------------------------------
// Type Guards
// ---------------------------------------------------------------------------

function isInstallStart(event: BaseEvent): event is InstallStartEvent {
  return event.eventType === 'INSTALL' && event.bound === 'START';
}

function isInstallEnd(event: BaseEvent): event is InstallEndEvent {
  return event.eventType === 'INSTALL' && event.bound === 'END';
}

function isUninstallStart(event: BaseEvent): event is UninstallStartEvent {
  return event.eventType === 'UNINSTALL' && event.bound === 'START';
}

function isUninstallEnd(event: BaseEvent): event is UninstallEndEvent {
  return event.eventType === 'UNINSTALL' && event.bound === 'END';
}

function isUpdateStart(event: BaseEvent): event is UpdateStartEvent {
  return event.eventType === 'UPDATE' && event.bound === 'START';
}

function isUpdateEnd(event: BaseEvent): event is UpdateEndEvent {
  return event.eventType === 'UPDATE' && event.bound === 'END';
}

function isPersistedData(event: BaseEvent): event is PersistedDataEvent {
  return event.eventType === 'PERSISTED_DATA' && event.bound === 'INSTANT';
}

function isQualifyStart(event: BaseEvent): event is QualifyStartEvent {
  return event.eventType === 'QUALIFY' && event.bound === 'START';
}

function isQualifyEnd(event: BaseEvent): event is QualifyEndEvent {
  return event.eventType === 'QUALIFY' && event.bound === 'END';
}

function isActivateStart(event: BaseEvent): event is ActivateStartEvent {
  return event.eventType === 'ACTIVATE' && event.bound === 'START';
}

function isActivateEnd(event: BaseEvent): event is ActivateEndEvent {
  return event.eventType === 'ACTIVATE' && event.bound === 'END';
}

function isPostRequestStart(event: BaseEvent): event is PostRequestStartEvent {
  return event.eventType === 'POST_REQUEST' && event.bound === 'START';
}

function isPostRequestEnd(event: BaseEvent): event is PostRequestEndEvent {
  return event.eventType === 'POST_REQUEST' && event.bound === 'END';
}

function isLoadPolicyStart(event: BaseEvent): event is LoadPolicyStartEvent {
  return event.eventType === 'LOAD_POLICY' && event.bound === 'START';
}

function isLoadPolicyEnd(event: BaseEvent): event is LoadPolicyEndEvent {
  return event.eventType === 'LOAD_POLICY' && event.bound === 'END';
}

function isUpdaterProcessStart(event: BaseEvent):
    event is UpdaterProcessStartEvent {
  return event.eventType === 'UPDATER_PROCESS' && event.bound === 'START';
}

function isUpdaterProcessEnd(event: BaseEvent):
    event is UpdaterProcessEndEvent {
  return event.eventType === 'UPDATER_PROCESS' && event.bound === 'END';
}

function isAppCommandStart(event: BaseEvent): event is AppCommandStartEvent {
  return event.eventType === 'APP_COMMAND' && event.bound === 'START';
}

function isAppCommandEnd(event: BaseEvent): event is AppCommandEndEvent {
  return event.eventType === 'APP_COMMAND' && event.bound === 'END';
}

/**
 * Type guard for MergedHistoryEvent.
 */
export function isMergedHistoryEvent(event: HistoryEvent|MergedHistoryEvent):
    event is MergedHistoryEvent {
  return 'startEvent' in event;
}

// ---------------------------------------------------------------------------
// Parsing
// ---------------------------------------------------------------------------

/**
 * An error indicating that a message could not be parsed as an event.
 */
class ParseError extends Error {}

/**
 * A wrapper for parsing functions that throws an error if the field is missing.
 */
function required<T>(
    parser: (
        message: Record<string, unknown>,
        fieldName: string,
        ) => T | undefined,
    message: Record<string, unknown>, fieldName: string): T {
  const parsed = parser(message, fieldName);
  if (parsed === undefined) {
    throw new ParseError(`Message missing required field '${fieldName}'`);
  }
  return parsed;
}

/**
 * Parses an integer field from a message. The field may be a natural integer or
 * the string representation an integer.
 */
function parseInteger(
    message: Record<string, unknown>, fieldName: string): number|undefined {
  const value = message[fieldName];
  if (value === undefined || value === null) {
    return undefined;
  }

  let num: number|undefined;
  switch (typeof value) {
    case 'number':
      num = value;
      break;
    case 'string':
      num = Number(value);
      break;
    default:
      throw new ParseError(
          `Message has field '${fieldName}' with unexpected type '${
              typeof value}', expected 'number' or 'string'`,
      );
  }

  if (!Number.isInteger(num)) {
    throw new ParseError(
        `Message has field '${
            fieldName}' with a numeric value that is not an integer.`,
    );
  }
  return num;
}

/**
 * Parses a string field from a message.
 */
function parseString(
    message: Record<string, unknown>, fieldName: string): string|undefined {
  const value = message[fieldName];
  if (value === undefined || value === null) {
    return undefined;
  }
  if (typeof value !== 'string') {
    throw new ParseError(
        `Message has field ${fieldName} with unexpected type '${
            typeof value}', expected 'string'`,
    );
  }
  return value;
}

/**
 * Parses a boolean field from a message.
 */
function parseBoolean(
    message: Record<string, unknown>, fieldName: string): boolean|undefined {
  const value = message[fieldName];
  if (value === undefined || value === null) {
    return undefined;
  }
  if (typeof value !== 'boolean') {
    throw new ParseError(
        `Message has field ${fieldName} with unexpected type '${
            typeof value}', expected 'boolean'`,
    );
  }
  return value;
}

/**
 * Parses an object field from a message.
 */
function parseObject(message: Record<string, unknown>, fieldName: string):
    Record<string, unknown>|undefined {
  const value = message[fieldName];
  if (value === undefined || value === null) {
    return undefined;
  }
  if (typeof value !== 'object' || Array.isArray(value)) {
    throw new ParseError(
        `Message has field ${fieldName} with unexpected type '${
            typeof value}', expected 'object'`,
    );
  }
  return value as Record<string, unknown>;
}

/**
 * Parses an EventType field from a message.
 */
function parseEventType(
    message: Record<string, unknown>, fieldName: string): EventType|undefined {
  const eventType = parseString(message, fieldName);
  if (eventType === undefined) {
    return undefined;
  }
  if (!EVENT_TYPES.includes(eventType as EventType)) {
    throw new ParseError(`Message contains unknown eventType: ${eventType}`);
  }
  return eventType as EventType;
}

/**
 * Parses a Bound field from a message.
 */
function parseBound(
    message: Record<string, unknown>, fieldName: string): Bound {
  const bound = parseString(message, fieldName) ?? 'INSTANT';
  if (!BOUNDS.includes(bound as Bound)) {
    throw new ParseError(`Message contains unknown bound: ${bound}`);
  }
  return bound as Bound;
}

/**
 * Parses an UpdaterError from a message.
 */
function parseUpdaterError(message: Record<string, unknown>): UpdaterError {
  const category = required(parseInteger, message, 'category');
  const code = required(parseInteger, message, 'code');
  const extracode1 = required(parseInteger, message, 'extracode1');
  return {category, code, extracode1};
}

/**
 * Parses a list of UpdaterErrors from a message.
 */
function parseUpdaterErrors(message: Record<string, unknown>): UpdaterError[] {
  const errors = message['errors'];
  // An absent error list is equivalent to an empty error list.
  if (errors === undefined || errors === null) {
    return [];
  }
  if (!Array.isArray(errors)) {
    throw new ParseError(
        `Message has field 'errors' of unexpected non-array type '${
            typeof errors}'.`,
    );
  }

  const parsedErrors: UpdaterError[] = [];
  for (const errorItem of errors) {
    if (typeof errorItem !== 'object' || errorItem === null) {
      throw new ParseError(
          `Message has field 'errors' containing an element of unexpected type '${
              typeof errorItem}', expected 'object'.`,
      );
    }
    if (Array.isArray(errorItem)) {
      throw new ParseError(
          `Message has field 'errors' of unexpected array type.`,
      );
    }
    parsedErrors.push(parseUpdaterError(errorItem as Record<string, unknown>));
  }
  return parsedErrors;
}

/**
 * Parses a RegisteredApp from a message.
 */
function parseRegisteredApp(message: Record<string, unknown>): RegisteredApp {
  const appId = required(parseString, message, 'appId');
  const version = required(parseString, message, 'version');
  const cohort = parseString(message, 'cohort');
  const brandCode = parseString(message, 'brandCode');
  return {appId, version, cohort, brandCode};
}

/**
 * Parses a list of RegisteredApps from a message.
 */
function parseRegisteredApps(
    message: Record<string, unknown>,
    fieldName: string,
    ): RegisteredApp[] {
  const registeredApps = message[fieldName];
  if (registeredApps === undefined || registeredApps === null) {
    return [];
  }
  if (!Array.isArray(registeredApps)) {
    throw new ParseError(
        `Message has field '${fieldName}' of unexpected non-array type '${
            typeof registeredApps}'.`,
    );
  }

  const parsedRegisteredApps: RegisteredApp[] = [];
  for (const appItem of registeredApps) {
    if (typeof appItem !== 'object' || appItem === null) {
      throw new ParseError(`Message has field '${
          fieldName}' containing an element of unexpected type '${
          typeof appItem}', expected 'object'.`);
    }
    if (Array.isArray(appItem)) {
      throw new ParseError(
          `Message has field '${fieldName}' of unexpected array type.`);
    }
    parsedRegisteredApps.push(
        parseRegisteredApp(appItem as Record<string, unknown>));
  }
  return parsedRegisteredApps;
}

/**
 * Parses an UpdatePriority field from a message.
 */
function parseUpdatePriority(
    message: Record<string, unknown>,
    fieldName: string,
    ): UpdatePriority|undefined {
  const priority = parseString(message, fieldName);
  if (priority === undefined) {
    return undefined;
  }
  if (!UPDATE_PRIORITIES.includes(priority as UpdatePriority)) {
    throw new ParseError(
        `Message contains unknown update priority: ${priority}`);
  }
  return priority as UpdatePriority;
}

/**
 * Parses a Scope field from a message.
 */
function parseScope(
    message: Record<string, unknown>,
    fieldName: string,
    ): Scope|undefined {
  const scope = parseString(message, fieldName);
  if (scope === undefined) {
    return undefined;
  }
  if (!SCOPES.includes(scope as Scope)) {
    throw new ParseError(`Message contains unknown scope: ${scope}`);
  }
  return scope as Scope;
}

/**
 * Parses PolicyData from a message.
 */
function parsePolicyData(message: Record<string, unknown>): PolicyData {
  const valuesBySource = required(parseObject, message, 'valuesBySource');
  const prevailingSource = required(parseString, message, 'prevailingSource');
  return {valuesBySource, prevailingSource};
}

/**
 * Parses a PolicySet from a message.
 */
export function parsePolicySet(
    message: Record<string, unknown>,
    fieldName: string,
    ): PolicySet|undefined {
  const policySet = parseObject(message, fieldName);
  if (policySet === undefined) {
    return undefined;
  }
  const policiesByNameMessage =
      required(parseObject, policySet, 'policiesByName');
  const policiesByName: {[key: string]: PolicyData} = {};
  for (const key of Object.keys(policiesByNameMessage)) {
    policiesByName[key] = parsePolicyData(
        required(parseObject, policiesByNameMessage, key),
    );
  }
  const policiesByAppIdMessage =
      required(parseObject, policySet, 'policiesByAppId');
  const policiesByAppId: {[key: string]: {[key: string]: PolicyData}} = {};
  for (const key of Object.keys(policiesByAppIdMessage)) {
    policiesByAppId[key] = {};
    const appPolicies = required(parseObject, policiesByAppIdMessage, key);
    for (const appKey of Object.keys(appPolicies)) {
      policiesByAppId[key][appKey] =
          parsePolicyData(required(parseObject, appPolicies, appKey));
    }
  }
  return {policiesByName, policiesByAppId};
}

// Microseconds between 1601-01-01 and 1970-01-01.
const WINDOWS_TO_UNIX_EPOCH_OFFSET = 11644473600000000n;

/**
 * Parses a Date field from a message. Dates are received as string-encoded
 * integer number of microseconds since Windows epoch.
 */
function parseDate(
    message: Record<string, unknown>,
    fieldName: string,
    ): Date|undefined {
  const dateStr = parseString(message, fieldName);
  if (dateStr === undefined) {
    return undefined;
  }
  let windowsMicroseconds;
  try {
    windowsMicroseconds = BigInt(dateStr);
  } catch (e) {
    throw new ParseError(`Message has field '${
        fieldName}' with unparsable datetime value '${dateStr}'`);
  }
  const unixMilliseconds =
      Number((windowsMicroseconds - WINDOWS_TO_UNIX_EPOCH_OFFSET) / 1000n);
  if (!Number.isFinite(unixMilliseconds)) {
    throw new ParseError(`Message has field '${
        fieldName}' with unparsable datetime value '${dateStr}'`);
  }
  return new Date(unixMilliseconds);
}

/**
 * Parses a BaseEvent from a message.
 */
function parseBaseEvent(message: Record<string, unknown>): BaseEvent {
  const eventType = required(parseEventType, message, 'eventType');
  const eventId = required(parseString, message, 'eventId');
  const deviceUptime = required(parseInteger, message, 'deviceUptime');
  const pid = required(parseInteger, message, 'pid');
  const processToken = required(parseString, message, 'processToken');
  const bound = parseBound(message, 'bound');
  const errors = parseUpdaterErrors(message);
  return {eventType, eventId, deviceUptime, pid, processToken, bound, errors};
}

function parseInstallStartEvent(
    base: StartEvent&{eventType: 'INSTALL'},
    message: Record<string, unknown>,
    ): InstallStartEvent {
  const appId = required(parseString, message, 'appId');
  return {...base, appId};
}

function parseInstallEndEvent(
    base: EndEvent&{eventType: 'INSTALL'},
    message: Record<string, unknown>,
    ): InstallEndEvent {
  const version = required(parseString, message, 'version');
  return {...base, version};
}

function parseUninstallStartEvent(
    base: StartEvent&{eventType: 'UNINSTALL'},
    message: Record<string, unknown>,
    ): UninstallStartEvent {
  const appId = required(parseString, message, 'appId');
  const version = required(parseString, message, 'version');
  const reason = required(parseString, message, 'reason');
  return {...base, appId, version, reason};
}

function parseUninstallEndEvent(
    base: EndEvent&{eventType: 'UNINSTALL'},
    _: Record<string, unknown>,
    ): UninstallEndEvent {
  return {...base};
}

function parseUpdateStartEvent(
    base: StartEvent&{eventType: 'UPDATE'},
    message: Record<string, unknown>,
    ): UpdateStartEvent {
  const appId = parseString(message, 'appId');
  const priority = parseUpdatePriority(message, 'priority');
  return {...base, appId, priority};
}

function parseUpdateEndEvent(
    base: EndEvent&{eventType: 'UPDATE'},
    message: Record<string, unknown>,
    ): UpdateEndEvent {
  const outcome = parseString(message, 'outcome');
  const nextVersion = parseString(message, 'nextVersion');
  return {...base, outcome, nextVersion};
}

function parseQualifyStartEvent(
    base: StartEvent&{eventType: 'QUALIFY'},
    _: Record<string, unknown>,
    ): QualifyStartEvent {
  return {...base};
}

function parseQualifyEndEvent(
    base: EndEvent&{eventType: 'QUALIFY'},
    message: Record<string, unknown>,
    ): QualifyEndEvent {
  const qualified = parseBoolean(message, 'qualified');
  return {...base, qualified};
}

function parseActivateStartEvent(
    base: StartEvent&{eventType: 'ACTIVATE'},
    _: Record<string, unknown>,
    ): ActivateStartEvent {
  return {...base};
}

function parseActivateEndEvent(
    base: EndEvent&{eventType: 'ACTIVATE'},
    message: Record<string, unknown>,
    ): ActivateEndEvent {
  const activated = parseBoolean(message, 'activated');
  return {...base, activated};
}

function parsePostRequestStartEvent(
    base: StartEvent&{eventType: 'POST_REQUEST'},
    message: Record<string, unknown>,
    ): PostRequestStartEvent {
  const request = required(parseString, message, 'request');
  return {...base, request};
}

function parsePostRequestEndEvent(
    base: EndEvent&{eventType: 'POST_REQUEST'},
    message: Record<string, unknown>,
    ): PostRequestEndEvent {
  const response = parseString(message, 'response');
  return {...base, response};
}

function parseLoadPolicyStartEvent(
    base: StartEvent&{eventType: 'LOAD_POLICY'},
    _: Record<string, unknown>,
    ): LoadPolicyStartEvent {
  return {...base};
}

function parseLoadPolicyEndEvent(
    base: EndEvent&{eventType: 'LOAD_POLICY'},
    message: Record<string, unknown>,
    ): LoadPolicyEndEvent {
  const policySet = parsePolicySet(message, 'policySet');
  return {...base, policySet};
}

function parseUpdaterProcessStartEvent(
    base: StartEvent&{eventType: 'UPDATER_PROCESS'},
    message: Record<string, unknown>,
    ): UpdaterProcessStartEvent {
  const commandLine = parseString(message, 'commandLine');
  const timestamp = parseDate(message, 'timestamp');
  const updaterVersion = parseString(message, 'updaterVersion');
  const scope = parseScope(message, 'scope');
  const osPlatform = parseString(message, 'osPlatform');
  const osVersion = parseString(message, 'osVersion');
  const osArchitecture = parseString(message, 'osArchitecture');
  const updaterArchitecture = parseString(message, 'updaterArchitecture');
  const parentPid = parseInteger(message, 'parentPid');
  return {
    ...base,
    commandLine,
    timestamp,
    updaterVersion,
    scope,
    osPlatform,
    osVersion,
    osArchitecture,
    updaterArchitecture,
    parentPid,
  };
}

function parseUpdaterProcessEndEvent(
    base: EndEvent&{eventType: 'UPDATER_PROCESS'},
    message: Record<string, unknown>): UpdaterProcessEndEvent {
  const exitCode = parseInteger(message, 'exitCode');
  return {...base, exitCode};
}

function parseAppCommandStartEvent(
    base: StartEvent&{eventType: 'APP_COMMAND'},
    message: Record<string, unknown>): AppCommandStartEvent {
  const appId = required(parseString, message, 'appId');
  const commandLine = parseString(message, 'commandLine');
  return {...base, appId, commandLine};
}

function parseAppCommandEndEvent(
    base: EndEvent&{eventType: 'APP_COMMAND'},
    message: Record<string, unknown>): AppCommandEndEvent {
  const exitCode = parseInteger(message, 'exitCode');
  const output = parseString(message, 'output');
  return {...base, exitCode, output};
}

function parsePersistedDataEvent(
    base: BaseEvent&{
      eventType: 'PERSISTED_DATA',
      bound: 'INSTANT',
    },
    message: Record<string, unknown>): PersistedDataEvent {
  const eulaRequired = required(parseBoolean, message, 'eulaRequired');
  const lastChecked = parseDate(message, 'lastChecked');
  const lastStarted = parseDate(message, 'lastStarted');
  const registeredApps = parseRegisteredApps(message, 'registeredApps');
  return {...base, eulaRequired, lastChecked, lastStarted, registeredApps};
}

/**
 * Parses a single event from a message.
 */
export function parseEvent(message: Record<string, unknown>): HistoryEvent {
  const base = parseBaseEvent(message);
  if (isInstallStart(base)) {
    return parseInstallStartEvent(base, message);
  }
  if (isInstallEnd(base)) {
    return parseInstallEndEvent(base, message);
  }
  if (isUninstallStart(base)) {
    return parseUninstallStartEvent(base, message);
  }
  if (isUninstallEnd(base)) {
    return parseUninstallEndEvent(base, message);
  }
  if (isUpdateStart(base)) {
    return parseUpdateStartEvent(base, message);
  }
  if (isUpdateEnd(base)) {
    return parseUpdateEndEvent(base, message);
  }
  if (isQualifyStart(base)) {
    return parseQualifyStartEvent(base, message);
  }
  if (isQualifyEnd(base)) {
    return parseQualifyEndEvent(base, message);
  }
  if (isActivateStart(base)) {
    return parseActivateStartEvent(base, message);
  }
  if (isActivateEnd(base)) {
    return parseActivateEndEvent(base, message);
  }
  if (isPostRequestStart(base)) {
    return parsePostRequestStartEvent(base, message);
  }
  if (isPostRequestEnd(base)) {
    return parsePostRequestEndEvent(base, message);
  }
  if (isLoadPolicyStart(base)) {
    return parseLoadPolicyStartEvent(base, message);
  }
  if (isLoadPolicyEnd(base)) {
    return parseLoadPolicyEndEvent(base, message);
  }
  if (isUpdaterProcessStart(base)) {
    return parseUpdaterProcessStartEvent(base, message);
  }
  if (isUpdaterProcessEnd(base)) {
    return parseUpdaterProcessEndEvent(base, message);
  }
  if (isAppCommandStart(base)) {
    return parseAppCommandStartEvent(base, message);
  }
  if (isAppCommandEnd(base)) {
    return parseAppCommandEndEvent(base, message);
  }
  if (isPersistedData(base)) {
    return parsePersistedDataEvent(base, message);
  }
  throw new ParseError(
      `No parser implemented for ${base.eventType} with bound ${base.bound}`);
}

/**
 * Parses a list of events from a list of messages.
 */
export function parseEvents(messages: Array<Record<string, unknown>>):
    {valid: HistoryEvent[], invalid: Array<Record<string, unknown>>} {
  const valid: HistoryEvent[] = [];
  const invalid: Array<Record<string, unknown>> = [];
  for (const message of messages) {
    try {
      valid.push(parseEvent(message));
    } catch (e) {
      if (e instanceof ParseError) {
        console.warn(`Failed to parse message: ${e}. Full message: ${message}`);
        invalid.push(message);
      } else {
        throw e;
      }
    }
  }
  return {valid, invalid};
}

// ---------------------------------------------------------------------------
// Parsed event processing
// ---------------------------------------------------------------------------

/**
 * Deduplicates events based on event type, event ID, PID, process token, and
 * bound.
 */
export function deduplicateEvents(events: HistoryEvent[]): HistoryEvent[] {
  const seen = new Set<string>();
  return events.filter((event) => {
    const key = JSON.stringify([
      event.eventType,
      event.eventId,
      event.pid,
      event.processToken,
      event.bound,
    ]);
    if (seen.has(key)) {
      return false;
    }
    seen.add(key);
    return true;
  });
}

/**
 * Merges START and END events into pairs
 */
export function mergeEvents(events: HistoryEvent[]):
    {paired: MergedHistoryEvent[], unpaired: HistoryEvent[]} {
  const unpaired: HistoryEvent[] = [];
  const pairs = new Map<string, {
    startEvent?: HistoryEvent & {bound: 'START'},
    endEvent?: HistoryEvent,
  }>();

  for (const event of events) {
    if (event.bound === 'INSTANT') {
      unpaired.push(event);
      continue;
    }

    const key = JSON.stringify([
      event.eventType,
      event.eventId,
      event.pid,
      event.processToken,
    ]);
    const pair = pairs.get(key) ?? {};

    if (event.bound === 'START') {
      if (pair.startEvent) {
        unpaired.push(event);
      } else {
        pair.startEvent = event;
      }
    } else if (event.bound === 'END') {
      if (pair.endEvent) {
        unpaired.push(event);
      } else {
        pair.endEvent = event;
      }
    }
    pairs.set(key, pair);
  }

  const paired: MergedHistoryEvent[] = [];
  for (const pair of pairs.values()) {
    if (pair.startEvent && pair.endEvent) {
      paired.push({
        eventType: pair.startEvent.eventType,
        startEvent: pair.startEvent,
        endEvent: pair.endEvent,
      } as MergedHistoryEvent);
    } else {
      if (pair.startEvent) {
        unpaired.push(pair.startEvent);
      }
      if (pair.endEvent) {
        unpaired.push(pair.endEvent);
      }
    }
  }

  return {paired, unpaired};
}

/**
 * Returns the lower-case app ID associated with an event, if any.
 */
export function getAppId(
    event: HistoryEvent|MergedHistoryEvent,
    ): string|undefined {
  const eventWithAppId = isMergedHistoryEvent(event) ? event.startEvent : event;
  return 'appId' in eventWithAppId ? eventWithAppId.appId?.toLowerCase() :
                                     undefined;
}

/**
 * A class to manage and query MergedUpdaterProcessEvent instances.
 * It builds a map of updater processes keyed by their process ID and token.
 */
export class UpdaterProcessMap {
  private readonly updaterProcesses =
      new Map<string, MergedUpdaterProcessEvent>();

  constructor(events: MergedHistoryEvent[]) {
    for (const event of events) {
      if (event.eventType === 'UPDATER_PROCESS') {
        // A merged event is ill-formed if it contains events from different
        // processes.
        console.assert(
            UpdaterProcessMap.eventProcessKey(event.startEvent) ===
            UpdaterProcessMap.eventProcessKey(event.endEvent));
        this.updaterProcesses.set(
            UpdaterProcessMap.eventProcessKey(event.startEvent), event);
      }
    }
  }

  /**
   * Retrieves the MergedUpdaterProcessEvent associated with a given event.
   * The lookup is based on the event's process ID and token. It can accept
   * either a merged event or an unmerged history event.
   */
  getUpdaterProcessForEvent(event: MergedHistoryEvent):
      MergedUpdaterProcessEvent|undefined;
  getUpdaterProcessForEvent(event: HistoryEvent): MergedUpdaterProcessEvent
      |undefined;
  getUpdaterProcessForEvent(event: MergedHistoryEvent|HistoryEvent):
      MergedUpdaterProcessEvent|undefined;
  getUpdaterProcessForEvent(event: MergedHistoryEvent|HistoryEvent):
      MergedUpdaterProcessEvent|undefined {
    const keyEvent = isMergedHistoryEvent(event) ? event.startEvent : event;
    return this.updaterProcesses.get(
        UpdaterProcessMap.eventProcessKey(keyEvent));
  }

  /**
   * Returns the date of the event, calculated from the updater process start
   * time and device uptime.
   */
  eventDate(event: MergedHistoryEvent): Date|undefined;
  eventDate(event: HistoryEvent): Date|undefined;
  eventDate(event: MergedHistoryEvent|HistoryEvent): Date|undefined;
  eventDate(event: MergedHistoryEvent|HistoryEvent): Date|undefined {
    const keyEvent: HistoryEvent =
        isMergedHistoryEvent(event) ? event.startEvent : event;
    const updaterProcess = this.getUpdaterProcessForEvent(keyEvent);
    if (updaterProcess === undefined ||
        updaterProcess.startEvent.timestamp === undefined) {
      return undefined;
    }

    // deviceUptime is in microseconds, Date.getTime() is in milliseconds.
    const startTimestampMs = updaterProcess.startEvent.timestamp.getTime();
    const startDeviceUptimeMs = updaterProcess.startEvent.deviceUptime / 1000;
    const keyEventDeviceUptimeMs = keyEvent.deviceUptime / 1000;
    return new Date(
        startTimestampMs - startDeviceUptimeMs + keyEventDeviceUptimeMs);
  }

  /**
   * Sorts events by date in descending order.
   * Events without dates are returned in a separate list.
   */
  sortEventsByDate(
      events: HistoryEvent[],
      mergedEvents: MergedHistoryEvent[],
      ): {
    sortedEventsWithDates: Array<HistoryEvent|MergedHistoryEvent>,
    unsortedEventsWithoutDates: Array<HistoryEvent|MergedHistoryEvent>,
  } {
    const allEvents: Array<HistoryEvent|MergedHistoryEvent> = [
      ...events,
      ...mergedEvents,
    ];
    const eventsWithDates:
        Array<{event: HistoryEvent | MergedHistoryEvent, date: Date}> = [];
    const unsortedEventsWithoutDates: Array<HistoryEvent|MergedHistoryEvent> =
        [];

    for (const event of allEvents) {
      const date = this.eventDate(event);
      if (date === undefined) {
        unsortedEventsWithoutDates.push(event);
      } else {
        eventsWithDates.push({event, date});
      }
    }

    eventsWithDates.sort((a, b) => b.date.getTime() - a.date.getTime());
    const sortedEventsWithDates = eventsWithDates.map((item) => item.event);
    return {sortedEventsWithDates, unsortedEventsWithoutDates};
  }

  /**
   * Determines the policy set which was effective for `event` by locating its
   * most recent LOAD_POLICY event.
   */
  effectivePolicySet(
      event: HistoryEvent|MergedHistoryEvent,
      events: Array<HistoryEvent|MergedHistoryEvent>): PolicySet|undefined {
    const process = this.getUpdaterProcessForEvent(event);
    if (process === undefined) {
      return undefined;
    }

    const eventTime = isMergedHistoryEvent(event) ?
        event.startEvent.deviceUptime :
        event.deviceUptime;
    const loadPolicyEvents = events.filter(
        (e): e is MergedLoadPolicyEvent => isMergedHistoryEvent(e) &&
            e.eventType === 'LOAD_POLICY' &&
            e.endEvent.policySet !== undefined &&
            e.startEvent.pid === process.startEvent.pid &&
            e.startEvent.processToken === process.startEvent.processToken &&
            e.startEvent.deviceUptime <= eventTime);
    if (loadPolicyEvents.length === 0) {
      return undefined;
    }
    return loadPolicyEvents
        .reduce(
            (prev, current) =>
                prev.endEvent.deviceUptime > current.endEvent.deviceUptime ?
                prev :
                current)
        .endEvent.policySet;
  }

  /**
   * Generates a key for an event unique to its process, based on its PID and
   * processToken.
   */
  private static eventProcessKey(event: HistoryEvent): string {
    return JSON.stringify([event.pid, event.processToken]);
  }
}
