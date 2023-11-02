// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Roel information for the Output module.
 */

import {OutputContextOrder} from './output_types.js';

/**
 * Metadata about supported automation roles.
 * @const {Object<{msgId: string,
 *                 earconId: (string|undefined),
 *                 inherits: (string|undefined),
 *                 verboseAncestry: (boolean|undefined),
 *                 contextOrder: (OutputContextOrder|undefined),
 *                 ignoreAncestry: (boolean|undefined)}>}
 * msgId: the message id of the role. Each role used requires a speech entry in
 *        chromevox_strings.grd + an optional Braille entry (with _BRL suffix).
 * earconId: an optional earcon to play when encountering the role.
 * inherits: inherits rules from this role.
 * contextOrder: where to place the context output.
 * ignoreAncestry: ignores ancestry (context) output for this role.
 * verboseAncestry: causes ancestry output to not reject duplicated roles. May
 * be desirable when wanting start and end span-like output.
 */
export const OutputRoleInfo = {
  abbr: {msgId: 'tag_abbr', inherits: 'abstractContainer'},
  alert: {msgId: 'role_alert'},
  alertDialog:
      {msgId: 'role_alertdialog', contextOrder: OutputContextOrder.FIRST},
  article: {msgId: 'role_article', inherits: 'abstractItem'},
  application: {msgId: 'role_application', inherits: 'abstractContainer'},
  audio: {msgId: 'tag_audio', inherits: 'abstractFormFieldContainer'},
  banner: {msgId: 'role_banner', inherits: 'abstractContainer'},
  button: {msgId: 'role_button', earconId: 'BUTTON'},
  buttonDropDown: {msgId: 'role_button', earconId: 'BUTTON'},
  checkBox: {msgId: 'role_checkbox'},
  columnHeader: {msgId: 'role_columnheader', inherits: 'cell'},
  comboBoxMenuButton: {msgId: 'role_combobox', earconId: 'LISTBOX'},
  comboBoxGrouping:
      {msgId: 'role_combobox', inherits: 'abstractFormFieldContainer'},
  comboBoxSelect: {
    msgId: 'role_button',
    earconId: 'POP_UP_BUTTON',
    inherits: 'comboBoxMenuButton',
  },
  complementary: {msgId: 'role_complementary', inherits: 'abstractContainer'},
  comment: {
    msgId: 'role_comment',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: 'abstractSpan',
  },
  contentDeletion: {
    msgId: 'role_content_deletion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: 'abstractSpan',
  },
  contentInsertion: {
    msgId: 'role_content_insertion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: 'abstractSpan',
  },
  contentInfo: {msgId: 'role_contentinfo', inherits: 'abstractContainer'},
  date: {msgId: 'input_type_date', inherits: 'abstractFormFieldContainer'},
  definition: {msgId: 'role_definition', inherits: 'abstractContainer'},
  descriptionList: {msgId: 'role_description_list', inherits: 'abstractList'},
  descriptionListDetail:
      {msgId: 'role_description_list_detail', inherits: 'abstractItem'},
  dialog: {
    msgId: 'role_dialog',
    contextOrder: OutputContextOrder.DIRECTED,
    ignoreAncestry: true,
  },
  directory: {msgId: 'role_directory', inherits: 'abstractContainer'},
  docAbstract: {msgId: 'role_doc_abstract', inherits: 'abstractSpan'},
  docAcknowledgments:
      {msgId: 'role_doc_acknowledgments', inherits: 'abstractSpan'},
  docAfterword: {msgId: 'role_doc_afterword', inherits: 'abstractContainer'},
  docAppendix: {msgId: 'role_doc_appendix', inherits: 'abstractSpan'},
  docBackLink:
      {msgId: 'role_doc_back_link', earconId: 'LINK', inherits: 'link'},
  docBiblioEntry: {
    msgId: 'role_doc_biblio_entry',
    earconId: 'LIST_ITEM',
    inherits: 'abstractItem',
  },
  docBibliography: {msgId: 'role_doc_bibliography', inherits: 'abstractSpan'},
  docBiblioRef:
      {msgId: 'role_doc_biblio_ref', earconId: 'LINK', inherits: 'link'},
  docChapter: {msgId: 'role_doc_chapter', inherits: 'abstractSpan'},
  docColophon: {msgId: 'role_doc_colophon', inherits: 'abstractSpan'},
  docConclusion: {msgId: 'role_doc_conclusion', inherits: 'abstractSpan'},
  docCover: {msgId: 'role_doc_cover', inherits: 'image'},
  docCredit: {msgId: 'role_doc_credit', inherits: 'abstractSpan'},
  docCredits: {msgId: 'role_doc_credits', inherits: 'abstractSpan'},
  docDedication: {msgId: 'role_doc_dedication', inherits: 'abstractSpan'},
  docEndnote: {
    msgId: 'role_doc_endnote',
    earconId: 'LIST_ITEM',
    inherits: 'abstractItem',
  },
  docEndnotes:
      {msgId: 'role_doc_endnotes', earconId: 'LISTBOX', inherits: 'list'},
  docEpigraph: {msgId: 'role_doc_epigraph', inherits: 'abstractSpan'},
  docEpilogue: {msgId: 'role_doc_epilogue', inherits: 'abstractSpan'},
  docErrata: {msgId: 'role_doc_errata', inherits: 'abstractSpan'},
  docExample: {msgId: 'role_doc_example', inherits: 'abstractSpan'},
  docFootnote: {
    msgId: 'role_doc_footnote',
    earconId: 'LIST_ITEM',
    inherits: 'abstractItem',
  },
  docForeword: {msgId: 'role_doc_foreword', inherits: 'abstractSpan'},
  docGlossary: {msgId: 'role_doc_glossary', inherits: 'abstractSpan'},
  docGlossRef:
      {msgId: 'role_doc_gloss_ref', earconId: 'LINK', inherits: 'link'},
  docIndex: {msgId: 'role_doc_index', inherits: 'abstractSpan'},
  docIntroduction: {msgId: 'role_doc_introduction', inherits: 'abstractSpan'},
  docNoteRef: {msgId: 'role_doc_note_ref', earconId: 'LINK', inherits: 'link'},
  docNotice: {msgId: 'role_doc_notice', inherits: 'abstractSpan'},
  docPageBreak: {msgId: 'role_doc_page_break', inherits: 'abstractSpan'},
  docPageFooter: {msgId: 'role_doc_page_footer', inherits: 'abstractSpan'},
  docPageHeader: {msgId: 'role_doc_page_header', inherits: 'abstractSpan'},
  docPageList: {msgId: 'role_doc_page_list', inherits: 'abstractSpan'},
  docPart: {msgId: 'role_doc_part', inherits: 'abstractSpan'},
  docPreface: {msgId: 'role_doc_preface', inherits: 'abstractSpan'},
  docPrologue: {msgId: 'role_doc_prologue', inherits: 'abstractSpan'},
  docPullquote: {msgId: 'role_doc_pullquote', inherits: 'abstractSpan'},
  docQna: {msgId: 'role_doc_qna', inherits: 'abstractSpan'},
  docSubtitle: {msgId: 'role_doc_subtitle', inherits: 'heading'},
  docTip: {msgId: 'role_doc_tip', inherits: 'abstractSpan'},
  docToc: {msgId: 'role_doc_toc', inherits: 'abstractSpan'},
  document: {msgId: 'role_document', inherits: 'abstractContainer'},
  form: {msgId: 'role_form', inherits: 'abstractContainer'},
  graphicsDocument:
      {msgId: 'role_graphics_document', inherits: 'abstractContainer'},
  graphicsObject:
      {msgId: 'role_graphics_object', inherits: 'abstractContainer'},
  graphicsSymbol: {msgId: 'role_graphics_symbol', inherits: 'image'},
  grid: {msgId: 'role_grid', inherits: 'table'},
  group: {msgId: 'role_group', inherits: 'abstractContainer'},
  heading: {
    msgId: 'role_heading',
  },
  image: {
    msgId: 'role_img',
  },
  imeCandidate: {msgId: 'ime_candidate', ignoreAncestry: true},
  inputTime: {msgId: 'input_type_time', inherits: 'abstractFormFieldContainer'},
  link: {msgId: 'role_link', earconId: 'LINK'},
  list: {msgId: 'role_list', inherits: 'abstractList'},
  listBox: {msgId: 'role_listbox', earconId: 'LISTBOX'},
  listBoxOption: {msgId: 'role_listitem', earconId: 'LIST_ITEM'},
  listGrid: {msgId: 'role_list_grid', inherits: 'table'},
  listItem:
      {msgId: 'role_listitem', earconId: 'LIST_ITEM', inherits: 'abstractItem'},
  log: {msgId: 'role_log', inherits: 'abstractNameFromContents'},
  main: {msgId: 'role_main', inherits: 'abstractContainer'},
  mark: {
    msgId: 'role_mark',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: 'abstractContainer',
  },
  marquee: {msgId: 'role_marquee', inherits: 'abstractNameFromContents'},
  math: {msgId: 'role_math', inherits: 'abstractContainer'},
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
  meter: {msgId: 'role_meter', inherits: 'abstractRange'},
  navigation: {msgId: 'role_navigation', inherits: 'abstractContainer'},
  note: {msgId: 'role_note', inherits: 'abstractContainer'},
  progressIndicator:
      {msgId: 'role_progress_indicator', inherits: 'abstractRange'},
  popUpButton: {
    msgId: 'role_button',
    earconId: 'POP_UP_BUTTON',
    inherits: 'comboBoxMenuButton',
  },
  radioButton: {msgId: 'role_radio'},
  radioGroup:
      {msgId: 'role_radiogroup', inherits: 'abstractFormFieldContainer'},
  region: {msgId: 'role_region', inherits: 'abstractContainer'},
  row: {msgId: 'role_row'},
  rowHeader: {msgId: 'role_rowheader', inherits: 'cell'},
  scrollBar: {msgId: 'role_scrollbar', inherits: 'abstractRange'},
  section: {msgId: 'role_region', inherits: 'abstractContainer'},
  search: {msgId: 'role_search', inherits: 'abstractContainer'},
  separator: {msgId: 'role_separator', inherits: 'abstractContainer'},
  slider: {msgId: 'role_slider', inherits: 'abstractRange', earconId: 'SLIDER'},
  spinButton: {
    msgId: 'role_spinbutton',
    inherits: 'abstractRange',
    earconId: 'LISTBOX',
  },
  splitter: {msgId: 'role_separator', inherits: 'abstractSpan'},
  status: {msgId: 'role_status', inherits: 'abstractNameFromContents'},
  subscript: {msgId: 'role_subscript', inherits: 'abstractSpan'},
  suggestion: {
    msgId: 'role_suggestion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: 'abstractSpan',
  },
  superscript: {msgId: 'role_superscript', inherits: 'abstractSpan'},
  tab: {msgId: 'role_tab', inherits: 'abstractContainer'},
  tabList: {msgId: 'role_tablist', inherits: 'abstractFormFieldContainer'},
  tabPanel: {msgId: 'role_tabpanel', inherits: 'abstractContainer'},
  searchBox: {msgId: 'role_search', earconId: 'EDITABLE_TEXT'},
  textField: {msgId: 'input_type_text', earconId: 'EDITABLE_TEXT'},
  textFieldWithComboBox: {msgId: 'role_combobox', earconId: 'EDITABLE_TEXT'},
  time: {msgId: 'tag_time', inherits: 'abstractFormFieldContainer'},
  timer: {msgId: 'role_timer', inherits: 'abstractNameFromContents'},
  toolbar: {msgId: 'role_toolbar', ignoreAncestry: true},
  toggleButton: {msgId: 'role_toggle_button', inherits: 'checkBox'},
  tree: {msgId: 'role_tree'},
  treeItem: {msgId: 'role_treeitem'},
  video: {msgId: 'tag_video', inherits: 'abstractFormFieldContainer'},
  window: {ignoreAncestry: true},
};
