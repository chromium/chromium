// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {sendWithPromise} from 'chrome://resources/js/cr.js';
import {getRequiredElement} from 'chrome://resources/js/util.js';

interface SandboxFeature {
  name: string;
  enabled: boolean;
}

interface BrowserHostProcess {
  processId: number;
  processType: string;
  name: string;
  metricsName: string;
  sandboxType: string;
}

interface RendererHostProcess {
  processId: number;
}

/**
 * This may have additional fields displayed in the JSON output.
 * See //sandbox/win/src/sandbox_constants.cc for keys in policy.
 */
interface PolicyDiagnostic {
  appContainerCapabilities: string[];
  appContainerSid: string;
  componentFilters: string;
  desiredIntegrityLevel: string;
  lockdownLevel: string;
  lowboxSid: string;
  platformMitigations: string;
  processId: number;
}

interface SandboxDiagnostics {
  browser: BrowserHostProcess[];
  renderer: RendererHostProcess[];
  policies: PolicyDiagnostic[];
  features: SandboxFeature[];
}

/**
 * Represents a mitigation field from the PROCESS_CREATION_MITITAGION_POLICY*
 * series in Winbase.h.
 */
abstract class MitigationField {
  mitigation: string;
  value: number;
  mask: number;
  offset: number;

  /**
   * mask & value must be 0<=x<=255.
   * @param mitigation human name of mitigation.
   * @param value value to match within mask.
   * @param mask applied before matching.
   * @param offset within PC section.
   */
  constructor(mitigation: string, value: number, mask: number, offset: number) {
    this.mitigation = mitigation;
    this.value = value;
    this.mask = mask;
    this.offset = offset;
  }

  /**
   * Each PC field overrides this as they know where their data is.
   * @param bytes platform mitigations data.
   * @return chunk containing this field or null.
   */
  abstract getFieldData(bytes: Uint8Array): Uint8Array|null;

  /**
   * Are all the bits of this field set in the mitigations represented by
   * |bytes|.
   * @param bytes platform mitigations.
   */
  isFieldSet(bytes: Uint8Array): boolean {
    if (bytes.length !== 4 && bytes.length !== 8 && bytes.length !== 16) {
      throw new Error('Platform mitigations has unexpected size');
    }
    const subfield = this.getFieldData(bytes);
    if (subfield == null || this.offset > subfield.length * 8) {
      return false;
    }
    const idx = subfield.length - 1 - Math.floor(this.offset / 8);
    const ibit = this.offset % 8;
    return (subfield[idx]! & (this.mask << ibit)) === (this.value << ibit);
  }
}

/**
 * PROCESS_CREATION_MITIGATION_POLICY legacy bits.
 */
class Pc0Field extends MitigationField {
  /**
   * @param bytes platform mitigations data.
   * @return chunk containing this field or null.
   */
  getFieldData(bytes: Uint8Array): Uint8Array {
    if (bytes.length === 4) {
      // Win32 only 4 bytes of fields.
      return bytes;
    } else if (bytes.length === 8) {
      return bytes;
    } else {
      return bytes.slice(0, 8);
    }
  }
}

/**
 * PROCESS_CREATION_MITIGATION_POLICY_*
 */
class Pc1Field extends MitigationField {
  getFieldData(bytes: Uint8Array) {
    if (bytes.length === 8) {
      return bytes;
    } else if (bytes.length === 16) {
      return bytes.slice(0, 8);
    }
    return null;
  }
}

/**
 * PROCESS_CREATION_MITIGATION_POLICY2_*
 */
class Pc2Field extends MitigationField {
  getFieldData(bytes: Uint8Array) {
    if (bytes.length === 8) {
      return null;
    } else if (bytes.length === 16) {
      return bytes.slice(8, 16);
    }
    return null;
  }
}

/**
 * Helper to show enabled mitigations from a stringified hex
  representation of PROCESS_CREATION_MITIGATION_POLICY_* entries.
 */
class DecodeMitigations {
  fields: MitigationField[];

  constructor() {
    this.fields = [
      // Defined in Windows.h from Winbase.h
      // basic (pc0) mitigations in {win7},{lsb of pc1}.
      new Pc0Field('DEP_ENABLE', 0x1, 0x01, 0),
      new Pc0Field('DEP_ATL_THUNK_ENABLE', 0x2, 0x02, 0),
      new Pc0Field('SEHOP_ENABLE', 0x4, 0x04, 0),

      // pc1 mitigations in {lsb of pc1}.
      new Pc1Field('FORCE_RELOCATE_IMAGES', 0x1, 0x03, 8),
      new Pc1Field('FORCE_RELOCATE_IMAGES_ALWAYS_OFF', 0x2, 0x03, 8),
      new Pc1Field('FORCE_RELOCATE_IMAGES_ALWAYS_ON_REQ_RELOCS', 0x3, 0x03, 8),
      new Pc1Field('HEAP_TERMINATE', 0x1, 0x03, 12),
      new Pc1Field('HEAP_TERMINATE_ALWAYS_OFF', 0x2, 0x03, 12),
      new Pc1Field('HEAP_TERMINATE_RESERVED', 0x3, 0x03, 12),
      new Pc1Field('BOTTOM_UP_ASLR', 0x1, 0x03, 16),
      new Pc1Field('BOTTOM_UP_ASLR_ALWAYS_OFF', 0x2, 0x03, 16),
      new Pc1Field('BOTTOM_UP_ASLR_RESERVED', 0x3, 0x03, 16),
      new Pc1Field('HIGH_ENTROPY_ASLR', 0x1, 0x03, 20),
      new Pc1Field('HIGH_ENTROPY_ASLR_ALWAYS_OFF', 0x2, 0x03, 20),
      new Pc1Field('HIGH_ENTROPY_ASLR_RESERVED', 0x3, 0x03, 20),
      new Pc1Field('STRICT_HANDLE_CHECKS', 0x1, 0x03, 24),
      new Pc1Field('STRICT_HANDLE_CHECKS_ALWAYS_OFF', 0x2, 0x03, 24),
      new Pc1Field('STRICT_HANDLE_CHECKS_RESERVED', 0x3, 0x03, 24),
      new Pc1Field('WIN32K_SYSTEM_CALL_DISABLE', 0x1, 0x03, 28),
      new Pc1Field('WIN32K_SYSTEM_CALL_DISABLE_ALWAYS_OFF', 0x2, 0x03, 28),
      new Pc1Field('WIN32K_SYSTEM_CALL_DISABLE_RESERVED', 0x3, 0x03, 28),
      new Pc1Field('EXTENSION_POINT_DISABLE', 0x1, 0x03, 32),
      new Pc1Field('EXTENSION_POINT_DISABLE_ALWAYS_OFF', 0x2, 0x03, 32),
      new Pc1Field('EXTENSION_POINT_DISABLE_RESERVED', 0x3, 0x03, 32),
      new Pc1Field('PROHIBIT_DYNAMIC_CODE', 0x1, 0x03, 36),
      new Pc1Field('PROHIBIT_DYNAMIC_CODE_ALWAYS_OFF', 0x2, 0x03, 36),
      new Pc1Field(
          'PROHIBIT_DYNAMIC_CODE_ALWAYS_ON_ALLOW_OPT_OUT', 0x3, 0x03, 36),
      new Pc1Field('CONTROL_FLOW_GUARD', 0x1, 0x03, 40),
      new Pc1Field('CONTROL_FLOW_GUARD_ALWAYS_OFF', 0x2, 0x03, 40),
      new Pc1Field('CONTROL_FLOW_GUARD_EXPORT_SUPPRESSION', 0x3, 0x03, 40),
      new Pc1Field('BLOCK_NON_MICROSOFT_BINARIES', 0x1, 0x03, 44),
      new Pc1Field('BLOCK_NON_MICROSOFT_BINARIES_ALWAYS_OFF', 0x2, 0x03, 44),
      new Pc1Field('BLOCK_NON_MICROSOFT_BINARIES_ALLOW_STORE', 0x3, 0x03, 44),
      new Pc1Field('FONT_DISABLE', 0x1, 0x03, 48),
      new Pc1Field('FONT_DISABLE_ALWAYS_OFF', 0x2, 0x03, 48),
      new Pc1Field('AUDIT_NONSYSTEM_FONTS', 0x3, 0x03, 48),
      new Pc1Field('IMAGE_LOAD_NO_REMOTE', 0x1, 0x03, 52),
      new Pc1Field('IMAGE_LOAD_NO_REMOTE_ALWAYS_OFF', 0x2, 0x03, 52),
      new Pc1Field('IMAGE_LOAD_NO_REMOTE_RESERVED', 0x3, 0x03, 52),
      new Pc1Field('IMAGE_LOAD_NO_LOW_LABEL', 0x1, 0x03, 56),
      new Pc1Field('IMAGE_LOAD_NO_LOW_LABEL_ALWAYS_OFF', 0x2, 0x03, 56),
      new Pc1Field('IMAGE_LOAD_NO_LOW_LABEL_RESERVED', 0x3, 0x03, 56),
      new Pc1Field('IMAGE_LOAD_PREFER_SYSTEM32', 0x1, 0x03, 60),
      new Pc1Field('IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_OFF', 0x2, 0x03, 60),
      new Pc1Field('IMAGE_LOAD_PREFER_SYSTEM32_RESERVED', 0x3, 0x03, 60),

      // pc2: in second 64bit block only.
      new Pc2Field('LOADER_INTEGRITY_CONTINUITY', 0x1, 0x03, 4),
      new Pc2Field('LOADER_INTEGRITY_CONTINUITY_ALWAYS_OFF', 0x2, 0x03, 4),
      new Pc2Field('LOADER_INTEGRITY_CONTINUITY_AUDIT', 0x3, 0x03, 4),
      new Pc2Field('STRICT_CONTROL_FLOW_GUARD', 0x1, 0x03, 8),
      new Pc2Field('STRICT_CONTROL_FLOW_GUARD_ALWAYS_OFF', 0x2, 0x03, 8),
      new Pc2Field('STRICT_CONTROL_FLOW_GUARD_RESERVED', 0x3, 0x03, 8),
      new Pc2Field('MODULE_TAMPERING_PROTECTION', 0x1, 0x03, 12),
      new Pc2Field('MODULE_TAMPERING_PROTECTION_ALWAYS_OFF', 0x2, 0x03, 12),
      new Pc2Field('MODULE_TAMPERING_PROTECTION_NOINHERIT', 0x3, 0x03, 12),
      new Pc2Field('RESTRICT_INDIRECT_BRANCH_PREDICTION', 0x1, 0x03, 16),
      new Pc2Field(
          'RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_OFF', 0x2, 0x03, 16),
      new Pc2Field(
          'RESTRICT_INDIRECT_BRANCH_PREDICTION_RESERVED', 0x3, 0x03, 16),
      new Pc2Field('ALLOW_DOWNGRADE_DYNAMIC_CODE_POLICY', 0x1, 0x03, 20),
      new Pc2Field(
          'ALLOW_DOWNGRADE_DYNAMIC_CODE_POLICY_ALWAYS_OFF', 0x2, 0x03, 20),
      new Pc2Field(
          'ALLOW_DOWNGRADE_DYNAMIC_CODE_POLICY_RESERVED', 0x3, 0x03, 20),
      new Pc2Field('SPECULATIVE_STORE_BYPASS_DISABLE', 0x1, 0x03, 24),
      new Pc2Field(
          'SPECULATIVE_STORE_BYPASS_DISABLE_ALWAYS_OFF', 0x2, 0x03, 24),
      new Pc2Field('SPECULATIVE_STORE_BYPASS_DISABLE_RESERVED', 0x3, 0x03, 24),
      new Pc2Field('CET_USER_SHADOW_STACKS', 0x1, 0x03, 28),
      new Pc2Field('CET_USER_SHADOW_STACKS_ALWAYS_OFF', 0x2, 0x03, 28),
      new Pc2Field('CET_USER_SHADOW_STACKS_STRICT_MODE', 0x3, 0x03, 28),
      new Pc2Field('USER_CET_SET_CONTEXT_IP_VALIDATION', 0x1, 0x03, 32),
      new Pc2Field(
          'USER_CET_SET_CONTEXT_IP_VALIDATION_ALWAYS_OFF', 0x2, 0x03, 32),
      new Pc2Field(
          'USER_CET_SET_CONTEXT_IP_VALIDATION_RELAXED_MODE', 0x3, 0x03, 32),
      new Pc2Field('BLOCK_NON_CET_BINARIES', 0x1, 0x03, 36),
      new Pc2Field('BLOCK_NON_CET_BINARIES_ALWAYS_OFF', 0x2, 0x03, 36),
      new Pc2Field('BLOCK_NON_CET_BINARIES_NON_EHCONT', 0x3, 0x03, 36),
      new Pc2Field('XTENDED_CONTROL_FLOW_GUARD', 0x1, 0x03, 40),
      new Pc2Field('XTENDED_CONTROL_FLOW_GUARD_ALWAYS_OFF', 0x2, 0x03, 40),
      new Pc2Field('XTENDED_CONTROL_FLOW_GUARD_RESERVED', 0x3, 0x03, 40),
      new Pc2Field('CET_DYNAMIC_APIS_OUT_OF_PROC_ONLY', 0x1, 0x03, 48),
      new Pc2Field(
          'CET_DYNAMIC_APIS_OUT_OF_PROC_ONLY_ALWAYS_OFF', 0x2, 0x03, 48),
      new Pc2Field('CET_DYNAMIC_APIS_OUT_OF_PROC_ONLY_RESERVED', 0x3, 0x03, 48),
      new Pc2Field('RESTRICT_CORE_SHARING', 0x1, 0x03, 52),
      new Pc2Field('RESTRICT_CORE_SHARING_ALWAYS_OFF', 0x2, 0x03, 52),
      new Pc2Field('RESTRICT_CORE_SHARING_RESERVED', 0x3, 0x03, 52),
      new Pc2Field('FSCTL_SYSTEM_CALL_DISABLE', 0x1, 0x03, 56),
      new Pc2Field('FSCTL_SYSTEM_CALL_DISABLE_ALWAYS_OFF', 0x2, 0x03, 56),
      new Pc2Field('FSCTL_SYSTEM_CALL_DISABLE_RESERVED', 0x3, 0x03, 56),
    ];
  }

  /**
   * @param str Hex encoded data.
   * @return bytes Decoded bytes.
   */
  parseHexString(str: string): Uint8Array {
    assert((str.length % 2 === 0), 'str must have even length');
    const bytes = new Uint8Array(str.length / 2);
    for (let idx = 0; idx < str.length / 2; idx++) {
      bytes[idx] = parseInt(str.slice(idx * 2, idx * 2 + 2), 16);
    }
    return bytes;
  }

  /**
   * Return a list of platform mitigation which are set in |mitigations|.
   * Mitigations will be in the same order as Winbase.h.
   * @param mitigations Hex encoded process mitigation flags.
   * @return Matched mitigation values.
   */
  enabledMitigations(mitigations: string): string[] {
    const bytes = this.parseHexString(mitigations);
    const output = [];
    for (const item of this.fields) {
      if (item.isFieldSet(bytes)) {
        output.push(item.mitigation);
      }
    }
    return output;
  }
}

const DECODE_MITIGATIONS = new DecodeMitigations();

const WELL_KNOWN_SIDS: {[sid: string]: string} = {
  'S-1-15-3-1': 'InternetClient',
  'S-1-15-3-2': 'InternetClientServer',
  'S-1-15-3-3': 'PrivateNetworkClientServer',
  'S-1-15-3-4': 'PicturesLibrary',
  'S-1-15-3-5': 'VideosLibrary',
  'S-1-15-3-6': 'MusicLibrary',
  'S-1-15-3-7': 'DocumentsLibrary',
  'S-1-15-3-8': 'EnterpriseAuthentication',
  'S-1-15-3-9': 'SharedUserCertificates',
  'S-1-15-3-10': 'RemovableStorage',
  'S-1-15-3-11': 'Appointments',
  'S-1-15-3-12': 'Contacts',
  'S-1-15-3-1024-3424233489-972189580-2057154623-747635277-1604371224-316187997-3786583170-1043257646':
      'chromeInstallFiles',
  'S-1-15-3-1024-1502825166-1963708345-2616377461-2562897074-4192028372-3968301570-1997628692-1435953622':
      'lpacAppExperience',
  'S-1-15-3-1024-2302894289-466761758-1166120688-1039016420-2430351297-4240214049-4028510897-3317428798':
      'lpacChromeInstallFiles',
  'S-1-15-3-1024-2405443489-874036122-4286035555-1823921565-1746547431-2453885448-3625952902-991631256':
      'lpacCom',
  'S-1-15-3-1024-3203351429-2120443784-2872670797-1918958302-2829055647-4275794519-765664414-2751773334':
      'lpacCryptoServices',
  'S-1-15-3-1024-126078593-3658686728-1984883306-821399696-3684079960-564038680-3414880098-3435825201':
      'lpacEnterprisePolicyChangeNotifications',
  'S-1-15-3-1024-1788129303-2183208577-3999474272-3147359985-1757322193-3815756386-151582180-1888101193':
      'lpacIdentityServices',
  'S-1-15-3-1024-3153509613-960666767-3724611135-2725662640-12138253-543910227-1950414635-4190290187':
      'lpacInstrumentation',
  'S-1-15-3-1024-1692970155-4054893335-185714091-3362601943-3526593181-1159816984-2199008581-497492991':
      'lpacMedia',
  'S-1-15-3-1024-220022770-701261984-3991292956-4208751020-2918293058-3396419331-1700932348-2078364891':
      'lpacPnPNotifications',
  'S-1-15-3-1024-528118966-3876874398-709513571-1907873084-3598227634-3698730060-278077788-3990600205':
      'lpacServicesManagement',
  'S-1-15-3-1024-1864111754-776273317-3666925027-2523908081-3792458206-3582472437-4114419977-1582884857':
      'lpacSessionManagement',
  'S-1-15-3-1024-1065365936-1281604716-3511738428-1654721687-432734479-3232135806-4053264122-3456934681':
      'registryRead',
};

/**
 * Maps capabilities to well known values.
 */
function mapCapabilitySid(sid: string): string {
  if (WELL_KNOWN_SIDS[sid]) {
    return WELL_KNOWN_SIDS[sid]!;
  }
  return sid;
}

/**
 * Adds a row to the sandbox-status table.
 */
function addRow(args: Node[]) {
  const row = document.createElement('tr');
  for (const td of args) {
    row.appendChild(td);
  }
  getRequiredElement('sandbox-status').appendChild(row);
}

/**
 * Makes a <td> containing arg as textContent.
 */
function makeTextEntry(textContent: string): Node {
  const col = document.createElement('td');
  col.textContent = textContent;
  return col;
}

/**
 * Makes a <td> containing formatted component filter flags.
 */
function makeComponentFilterEntry(policy: PolicyDiagnostic): Node {
  const fixed = document.createElement('div');
  fixed.classList.add('mitigations');
  fixed.innerText = policy.componentFilters;
  const col = document.createElement('td');
  col.appendChild(fixed);
  return col;
}

/**
 * Makes an expandable <td> containing arg as textContent.
 */
function makeExpandableEntry(mainEntry: string, expandable: Expandable): Node {
  const button = document.createElement('div');
  const expand = document.createElement('div');
  button.innerText = '\u2795';  // (+)
  button.classList.add('expander');
  button.addEventListener('click', function() {
    if (expandable.onClick(expand)) {
      button.innerText = '\u2796';  // (-)
    } else {
      button.innerText = '\u2795';  // (+)
    }
  });
  const fixed = document.createElement('div');
  fixed.classList.add('mitigations');
  fixed.innerText = mainEntry;

  const col = document.createElement('td');
  col.appendChild(button);
  col.appendChild(fixed);
  col.appendChild(expand);
  return col;
}

abstract class Expandable {
  expanded: boolean = false;

  onClick(col: HTMLElement): boolean {
    this.expanded = !this.expanded;
    col.innerText = this.getText();
    return this.expanded;
  }

  abstract getText(): string;
}

class MitigationEntryExpandable extends Expandable {
  mitigations: string;

  constructor(mitigations: string) {
    super();
    this.mitigations = mitigations;
  }

  override getText(): string {
    if (this.expanded) {
      return DECODE_MITIGATIONS.enabledMitigations(this.mitigations).join('\n');
    } else {
      return '';
    }
  }
}

class AppContainerEntryExpandable extends Expandable {
  caps: string[];

  constructor(caps: string[]) {
    super();
    this.caps = caps;
  }

  override getText(): string {
    if (this.expanded) {
      return this.caps.map(mapCapabilitySid).sort().join('\n');
    } else {
      return '';
    }
  }
}

/**
 * Adds a mitigations entry that can expand to show friendly names of the
 * mitigations.
 */
function makeMitigationEntry(platformMitigations: string): Node {
  const expander = new MitigationEntryExpandable(platformMitigations);
  return makeExpandableEntry(platformMitigations, expander);
}

/**
 * Formats a lowbox sid or appcontainer configuration (policies can only
 * have one or the other).
 */
function makeLowboxAcEntry(policy: PolicyDiagnostic): Node {
  if (policy.lowboxSid) {
    // Lowbox token does not have capabilities but should match AC entries.
    const fixed = document.createElement('div');
    fixed.classList.add('mitigations');
    fixed.innerText = policy.lowboxSid;
    const col = document.createElement('td');
    col.appendChild(fixed);
    return col;
  }
  if (policy.appContainerSid) {
    // AC has identifying SID plus lockdown capabilities.
    const expander =
        new AppContainerEntryExpandable(policy.appContainerCapabilities);
    return makeExpandableEntry(policy.appContainerSid, expander);
  }
  return makeTextEntry('');
}

/**
 * Adds policy information for a process to the sandbox-status table.
 */
function addRowForProcess(
    pid: number, type: string, name: string, sandbox: string,
    policy: PolicyDiagnostic) {
  if (policy) {
    // Text-only items.
    const entries = [
      String(pid),
      type,
      name,
      sandbox,
      policy.lockdownLevel,
      policy.desiredIntegrityLevel,
    ].map(makeTextEntry);
    entries.push(makeMitigationEntry(policy.platformMitigations));
    entries.push(makeComponentFilterEntry(policy));
    entries.push(makeLowboxAcEntry(policy));
    addRow(entries);
  } else {
    addRow([String(pid), type, name, 'Not Sandboxed', '', '', '', '', ''].map(
        makeTextEntry));
  }
}

function onGetSandboxDiagnostics(results: SandboxDiagnostics) {
  // Make it easy to look up policies.
  const policies: Map<number, PolicyDiagnostic> = new Map();
  for (const policy of results.policies) {
    policies.set(policy.processId, policy);
  }

  // Titles.
  addRow([
    'Process',
    'Type',
    'Name',
    'Sandbox',
    'Lockdown',
    'Integrity',
    'Mitigations',
    'Component Filter',
    'Lowbox/AppContainer',
  ].map(makeTextEntry));

  // Browser Processes.
  for (const process of results.browser) {
    const pid = process.processId;
    const name = process.name || process.metricsName;
    addRowForProcess(
        pid,
        process.processType,
        name,
        process.sandboxType,
        policies.get(pid)!,
    );
  }

  // Renderer Processes.
  for (const process of results.renderer) {
    const pid = process.processId;
    addRowForProcess(pid, 'Renderer', '', 'Renderer', policies.get(pid)!);
  }

  // Raw Diagnostics.
  getRequiredElement('raw-info').textContent =
      'features: ' + JSON.stringify(results.features, null, 2) + '\n' +
      'policies: ' + JSON.stringify(results.policies, null, 2);
}

document.addEventListener('DOMContentLoaded', () => {
  sendWithPromise('requestSandboxDiagnostics').then(onGetSandboxDiagnostics);
});
