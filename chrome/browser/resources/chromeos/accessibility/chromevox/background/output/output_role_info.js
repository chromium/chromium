// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Roel information for the Output module.
 */

import {Earcon} from '../../common/abstract_earcons.js';
import {AbstractRole, ChromeVoxRole} from '../../common/role_type.js';

import {OutputContextOrder} from './output_types.js';

const RoleType = chrome.automation.RoleType;

/**
 * Metadata about supported automation roles.
 * @const {Object<{msgId: string,
 *                 earcon: (!Earcon|undefined),
 *                 inherits: (ChromeVoxRole|undefined),
 *                 verboseAncestry: (boolean|undefined),
 *                 contextOrder: (OutputContextOrder|undefined),
 *                 ignoreAncestry: (boolean|undefined)}>}
 * msgId: the message id of the role. Each role used requires a speech entry in
 *        chromevox_strings.grd + an optional Braille entry (with _BRL suffix).
 * earcon: an optional earcon to play when encountering the role.
 * inherits: inherits rules from this role.
 * contextOrder: where to place the context output.
 * ignoreAncestry: ignores ancestry (context) output for this role.
 * verboseAncestry: causes ancestry output to not reject duplicated roles. May
 * be desirable when wanting start and end span-like output.
 */
export const OutputRoleInfo = {
  abbr: {msgId: 'tag_abbr', inherits: AbstractRole.CONTAINER},
  alert: {msgId: 'role_alert'},
  alertDialog:
      {msgId: 'role_alertdialog', contextOrder: OutputContextOrder.FIRST},
  article: {msgId: 'role_article', inherits: AbstractRole.ITEM},
  application: {msgId: 'role_application', inherits: AbstractRole.CONTAINER},
  audio: {msgId: 'tag_audio', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  banner: {msgId: 'role_banner', inherits: AbstractRole.CONTAINER},
  button: {msgId: 'role_button', earcon: Earcon.BUTTON},
  buttonDropDown: {msgId: 'role_button', earcon: Earcon.BUTTON},
  checkBox: {msgId: 'role_checkbox'},
  columnHeader: {msgId: 'role_columnheader', inherits: RoleType.CELL},
  comboBoxMenuButton: {msgId: 'role_combobox', earcon: Earcon.LISTBOX},
  comboBoxGrouping:
      {msgId: 'role_combobox', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  comboBoxSelect: {
    msgId: 'role_button',
    earcon: Earcon.POP_UP_BUTTON,
    inherits: RoleType.COMBO_BOX_MENU_BUTTON,
  },
  complementary:
      {msgId: 'role_complementary', inherits: AbstractRole.CONTAINER},
  comment: {
    msgId: 'role_comment',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.SPAN,
  },
  contentDeletion: {
    msgId: 'role_content_deletion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.SPAN,
  },
  contentInsertion: {
    msgId: 'role_content_insertion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.SPAN,
  },
  contentInfo: {msgId: 'role_contentinfo', inherits: AbstractRole.CONTAINER},
  date: {msgId: 'input_type_date', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  definition: {msgId: 'role_definition', inherits: AbstractRole.CONTAINER},
  descriptionList:
      {msgId: 'role_description_list', inherits: AbstractRole.LIST},
  descriptionListDetail:
      {msgId: 'role_description_list_detail', inherits: AbstractRole.ITEM},
  dialog: {
    msgId: 'role_dialog',
    contextOrder: OutputContextOrder.DIRECTED,
    ignoreAncestry: true,
  },
  directory: {msgId: 'role_directory', inherits: AbstractRole.CONTAINER},
  docAbstract: {msgId: 'role_doc_abstract', inherits: AbstractRole.SPAN},
  docAcknowledgments:
      {msgId: 'role_doc_acknowledgments', inherits: AbstractRole.SPAN},
  docAfterword: {msgId: 'role_doc_afterword', inherits: AbstractRole.CONTAINER},
  docAppendix: {msgId: 'role_doc_appendix', inherits: AbstractRole.SPAN},
  docBackLink: {
    msgId: 'role_doc_back_link',
    earcon: Earcon.LINK,
    inherits: RoleType.LINK,
  },
  docBiblioEntry: {
    msgId: 'role_doc_biblio_entry',
    earcon: Earcon.LIST_ITEM,
    inherits: AbstractRole.ITEM,
  },
  docBibliography:
      {msgId: 'role_doc_bibliography', inherits: AbstractRole.SPAN},
  docBiblioRef: {
    msgId: 'role_doc_biblio_ref',
    earcon: Earcon.LINK,
    inherits: RoleType.LINK,
  },
  docChapter: {msgId: 'role_doc_chapter', inherits: AbstractRole.SPAN},
  docColophon: {msgId: 'role_doc_colophon', inherits: AbstractRole.SPAN},
  docConclusion: {msgId: 'role_doc_conclusion', inherits: AbstractRole.SPAN},
  docCover: {msgId: 'role_doc_cover', inherits: RoleType.IMAGE},
  docCredit: {msgId: 'role_doc_credit', inherits: AbstractRole.SPAN},
  docCredits: {msgId: 'role_doc_credits', inherits: AbstractRole.SPAN},
  docDedication: {msgId: 'role_doc_dedication', inherits: AbstractRole.SPAN},
  docEndnote: {
    msgId: 'role_doc_endnote',
    earcon: Earcon.LIST_ITEM,
    inherits: AbstractRole.ITEM,
  },
  docEndnotes: {
    msgId: 'role_doc_endnotes',
    earcon: Earcon.LISTBOX,
    inherits: RoleType.LIST,
  },
  docEpigraph: {msgId: 'role_doc_epigraph', inherits: AbstractRole.SPAN},
  docEpilogue: {msgId: 'role_doc_epilogue', inherits: AbstractRole.SPAN},
  docErrata: {msgId: 'role_doc_errata', inherits: AbstractRole.SPAN},
  docExample: {msgId: 'role_doc_example', inherits: AbstractRole.SPAN},
  docFootnote: {
    msgId: 'role_doc_footnote',
    earcon: Earcon.LIST_ITEM,
    inherits: AbstractRole.ITEM,
  },
  docForeword: {msgId: 'role_doc_foreword', inherits: AbstractRole.SPAN},
  docGlossary: {msgId: 'role_doc_glossary', inherits: AbstractRole.SPAN},
  docGlossRef: {
    msgId: 'role_doc_gloss_ref',
    earcon: Earcon.LINK,
    inherits: RoleType.LINK,
  },
  docIndex: {msgId: 'role_doc_index', inherits: AbstractRole.SPAN},
  docIntroduction:
      {msgId: 'role_doc_introduction', inherits: AbstractRole.SPAN},
  docNoteRef: {
    msgId: 'role_doc_note_ref',
    earcon: Earcon.LINK,
    inherits: RoleType.LINK,
  },
  docNotice: {msgId: 'role_doc_notice', inherits: AbstractRole.SPAN},
  docPageBreak: {msgId: 'role_doc_page_break', inherits: AbstractRole.SPAN},
  docPageFooter: {msgId: 'role_doc_page_footer', inherits: AbstractRole.SPAN},
  docPageHeader: {msgId: 'role_doc_page_header', inherits: AbstractRole.SPAN},
  docPageList: {msgId: 'role_doc_page_list', inherits: AbstractRole.SPAN},
  docPart: {msgId: 'role_doc_part', inherits: AbstractRole.SPAN},
  docPreface: {msgId: 'role_doc_preface', inherits: AbstractRole.SPAN},
  docPrologue: {msgId: 'role_doc_prologue', inherits: AbstractRole.SPAN},
  docPullquote: {msgId: 'role_doc_pullquote', inherits: AbstractRole.SPAN},
  docQna: {msgId: 'role_doc_qna', inherits: AbstractRole.SPAN},
  docSubtitle: {msgId: 'role_doc_subtitle', inherits: RoleType.HEADING},
  docTip: {msgId: 'role_doc_tip', inherits: AbstractRole.SPAN},
  docToc: {msgId: 'role_doc_toc', inherits: AbstractRole.SPAN},
  document: {msgId: 'role_document', inherits: AbstractRole.CONTAINER},
  form: {msgId: 'role_form', inherits: AbstractRole.CONTAINER},
  graphicsDocument:
      {msgId: 'role_graphics_document', inherits: AbstractRole.CONTAINER},
  graphicsObject:
      {msgId: 'role_graphics_object', inherits: AbstractRole.CONTAINER},
  graphicsSymbol: {msgId: 'role_graphics_symbol', inherits: RoleType.IMAGE},
  grid: {msgId: 'role_grid', inherits: RoleType.TABLE},
  group: {msgId: 'role_group', inherits: AbstractRole.CONTAINER},
  heading: {
    msgId: 'role_heading',
  },
  image: {
    msgId: 'role_img',
  },
  imeCandidate: {msgId: 'ime_candidate', ignoreAncestry: true},
  inputTime:
      {msgId: 'input_type_time', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  link: {msgId: 'role_link', earcon: Earcon.LINK},
  list: {msgId: 'role_list', inherits: AbstractRole.LIST},
  listBox: {msgId: 'role_listbox', earcon: Earcon.LISTBOX},
  listBoxOption: {msgId: 'role_listitem', earcon: Earcon.LIST_ITEM},
  listGrid: {msgId: 'role_list_grid', inherits: RoleType.TABLE},
  listItem: {
    msgId: 'role_listitem',
    earcon: Earcon.LIST_ITEM,
    inherits: AbstractRole.ITEM,
  },
  log: {msgId: 'role_log', inherits: AbstractRole.NAME_FROM_CONTENTS},
  main: {msgId: 'role_main', inherits: AbstractRole.CONTAINER},
  mark: {
    msgId: 'role_mark',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.CONTAINER,
  },
  marquee: {msgId: 'role_marquee', inherits: AbstractRole.NAME_FROM_CONTENTS},
  math: {msgId: 'role_math', inherits: AbstractRole.CONTAINER},
  menu: {
    msgId: 'role_menu',
    contextOrder: OutputContextOrder.FIRST,
    ignoreAncestry: true,
  },
  menuBar: {
    msgId: 'role_menubar',
  },
  menuItem: {msgId: 'role_menuitem'},
  menuItemCheckBox: {msgId: 'role_menuitemcheckbox'},
  menuItemRadio: {msgId: 'role_menuitemradio'},
  menuListOption: {msgId: 'role_menuitem'},
  menuListPopup: {msgId: 'role_menu'},
  meter: {msgId: 'role_meter', inherits: AbstractRole.RANGE},
  navigation: {msgId: 'role_navigation', inherits: AbstractRole.CONTAINER},
  note: {msgId: 'role_note', inherits: AbstractRole.CONTAINER},
  progressIndicator:
      {msgId: 'role_progress_indicator', inherits: AbstractRole.RANGE},
  popUpButton: {
    msgId: 'role_button',
    earcon: Earcon.POP_UP_BUTTON,
    inherits: RoleType.COMBO_BOX_MENU_BUTTON,
  },
  radioButton: {msgId: 'role_radio'},
  radioGroup:
      {msgId: 'role_radiogroup', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  region: {msgId: 'role_region', inherits: AbstractRole.CONTAINER},
  row: {msgId: 'role_row'},
  rowHeader: {msgId: 'role_rowheader', inherits: RoleType.CELL},
  scrollBar: {msgId: 'role_scrollbar', inherits: AbstractRole.RANGE},
  section: {msgId: 'role_region', inherits: AbstractRole.CONTAINER},
  search: {msgId: 'role_search', inherits: AbstractRole.CONTAINER},
  separator: {msgId: 'role_separator', inherits: AbstractRole.CONTAINER},
  slider: {
    msgId: 'role_slider',
    inherits: AbstractRole.RANGE,
    earcon: Earcon.SLIDER,
  },
  spinButton: {
    msgId: 'role_spinbutton',
    inherits: AbstractRole.RANGE,
    earcon: Earcon.LISTBOX,
  },
  splitter: {msgId: 'role_separator', inherits: AbstractRole.SPAN},
  status: {msgId: 'role_status', inherits: AbstractRole.NAME_FROM_CONTENTS},
  subscript: {msgId: 'role_subscript', inherits: AbstractRole.SPAN},
  suggestion: {
    msgId: 'role_suggestion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.SPAN,
  },
  superscript: {msgId: 'role_superscript', inherits: AbstractRole.SPAN},
  tab: {msgId: 'role_tab', inherits: AbstractRole.CONTAINER},
  tabList: {msgId: 'role_tablist', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  tabPanel: {msgId: 'role_tabpanel', inherits: AbstractRole.CONTAINER},
  searchBox: {msgId: 'role_search', earcon: Earcon.EDITABLE_TEXT},
  textField: {msgId: 'input_type_text', earcon: Earcon.EDITABLE_TEXT},
  textFieldWithComboBox: {msgId: 'role_combobox', earcon: Earcon.EDITABLE_TEXT},
  time: {msgId: 'tag_time', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  timer: {msgId: 'role_timer', inherits: AbstractRole.NAME_FROM_CONTENTS},
  toolbar: {msgId: 'role_toolbar', ignoreAncestry: true},
  toggleButton: {msgId: 'role_toggle_button', inherits: RoleType.CHECK_BOX},
  tree: {msgId: 'role_tree'},
  treeItem: {msgId: 'role_treeitem'},
  video: {msgId: 'tag_video', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  window: {ignoreAncestry: true},
};
