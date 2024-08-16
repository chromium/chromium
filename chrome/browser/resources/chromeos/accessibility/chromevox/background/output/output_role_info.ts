// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Roel information for the Output module.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

import {EarconId} from '../../common/earcon_id.js';
import {AbstractRole, ChromeVoxRole} from '../../common/role_type.js';

import {OutputContextOrder} from './output_types.js';

import RoleType = chrome.automation.RoleType;

/**
 * Metadata about supported automation roles.
 * msgId: the message id of the role. Each role used requires a speech entry in
 *        chromevox_strings.grd + an optional Braille entry (with _BRL suffix).
 * earcon: an optional earcon to play when encountering the role.
 * inherits: inherits rules from this role.
 * contextOrder: where to place the context output.
 * ignoreAncestry: ignores ancestry (context) output for this role.
 * verboseAncestry: causes ancestry output to not reject duplicated roles. May
 * be desirable when wanting start and end span-like output.
 */
export interface Info {
  contextOrder?: OutputContextOrder;
  earcon?: EarconId;
  ignoreAncestry?: boolean;
  inherits?: ChromeVoxRole;
  msgId?: string;
  verboseAncestry?: boolean;
}

export const OutputRoleInfo: Partial<Record<RoleType, Info>> = {
  [RoleType.ABBR]: {msgId: 'tag_abbr', inherits: AbstractRole.CONTAINER},
  [RoleType.ALERT]: {msgId: 'role_alert'},
  [RoleType.ALERT_DIALOG]:
      {msgId: 'role_alertdialog', contextOrder: OutputContextOrder.FIRST},
  [RoleType.ARTICLE]: {msgId: 'role_article', inherits: AbstractRole.ITEM},
  [RoleType.APPLICATION]:
      {msgId: 'role_application', inherits: AbstractRole.CONTAINER},
  [RoleType.AUDIO]:
      {msgId: 'tag_audio', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  [RoleType.BANNER]: {msgId: 'role_banner', inherits: AbstractRole.CONTAINER},
  [RoleType.BUTTON]: {msgId: 'role_button', earcon: EarconId.BUTTON},
  [RoleType.CHECK_BOX]: {msgId: 'role_checkbox'},
  [RoleType.COLUMN_HEADER]:
      {msgId: 'role_columnheader', inherits: RoleType.CELL},
  [RoleType.COMBO_BOX_GROUPING]:
      {msgId: 'role_combobox', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  [RoleType.COMBO_BOX_MENU_BUTTON]:
      {msgId: 'role_combobox', earcon: EarconId.LISTBOX},
  [RoleType.COMBO_BOX_SELECT]: {
    msgId: 'role_button',
    earcon: EarconId.POP_UP_BUTTON,
    inherits: RoleType.COMBO_BOX_MENU_BUTTON,
  },
  [RoleType.COMMENT]: {
    msgId: 'role_comment',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.SPAN,
  },
  [RoleType.COMPLEMENTARY]:
      {msgId: 'role_complementary', inherits: AbstractRole.CONTAINER},
  [RoleType.CONTENT_DELETION]: {
    msgId: 'role_content_deletion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.SPAN,
  },
  [RoleType.CONTENT_INSERTION]: {
    msgId: 'role_content_insertion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.SPAN,
  },
  [RoleType.CONTENT_INFO]:
      {msgId: 'role_contentinfo', inherits: AbstractRole.CONTAINER},
  [RoleType.DATE]:
      {msgId: 'input_type_date', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  [RoleType.DEFINITION]:
      {msgId: 'role_definition', inherits: AbstractRole.CONTAINER},
  [RoleType.DESCRIPTION_LIST]:
      {msgId: 'role_description_list', inherits: AbstractRole.LIST},
  [RoleType.DESCRIPTION_LIST_DETAIL_DEPRECATED]:
      {msgId: 'role_description_list_detail', inherits: AbstractRole.ITEM},
  [RoleType.DIALOG]: {
    msgId: 'role_dialog',
    contextOrder: OutputContextOrder.DIRECTED,
    ignoreAncestry: true,
  },
  [RoleType.DIRECTORY_DEPRECATED]:
      {msgId: 'role_directory', inherits: AbstractRole.CONTAINER},
  [RoleType.DOC_ABSTRACT]:
      {msgId: 'role_doc_abstract', inherits: AbstractRole.SPAN},
  [RoleType.DOC_ACKNOWLEDGMENTS]:
      {msgId: 'role_doc_acknowledgments', inherits: AbstractRole.SPAN},
  [RoleType.DOC_AFTERWORD]:
      {msgId: 'role_doc_afterword', inherits: AbstractRole.CONTAINER},
  [RoleType.DOC_APPENDIX]:
      {msgId: 'role_doc_appendix', inherits: AbstractRole.SPAN},
  [RoleType.DOC_BACK_LINK]: {
    msgId: 'role_doc_back_link',
    earcon: EarconId.LINK,
    inherits: RoleType.LINK,
  },
  [RoleType.DOC_BIBLIO_ENTRY]: {
    msgId: 'role_doc_biblio_entry',
    earcon: EarconId.LIST_ITEM,
    inherits: AbstractRole.ITEM,
  },
  [RoleType.DOC_BIBLIOGRAPHY]:
      {msgId: 'role_doc_bibliography', inherits: AbstractRole.SPAN},
  [RoleType.DOC_BIBLIO_REF]: {
    msgId: 'role_doc_biblio_ref',
    earcon: EarconId.LINK,
    inherits: RoleType.LINK,
  },
  [RoleType.DOC_CHAPTER]:
      {msgId: 'role_doc_chapter', inherits: AbstractRole.SPAN},
  [RoleType.DOC_COLOPHON]:
      {msgId: 'role_doc_colophon', inherits: AbstractRole.SPAN},
  [RoleType.DOC_CONCLUSION]:
      {msgId: 'role_doc_conclusion', inherits: AbstractRole.SPAN},
  [RoleType.DOC_COVER]: {msgId: 'role_doc_cover', inherits: RoleType.IMAGE},
  [RoleType.DOC_CREDIT]:
      {msgId: 'role_doc_credit', inherits: AbstractRole.SPAN},
  [RoleType.DOC_CREDITS]:
      {msgId: 'role_doc_credits', inherits: AbstractRole.SPAN},
  [RoleType.DOC_DEDICATION]:
      {msgId: 'role_doc_dedication', inherits: AbstractRole.SPAN},
  [RoleType.DOC_ENDNOTE]: {
    msgId: 'role_doc_endnote',
    earcon: EarconId.LIST_ITEM,
    inherits: AbstractRole.ITEM,
  },
  [RoleType.DOC_ENDNOTES]: {
    msgId: 'role_doc_endnotes',
    earcon: EarconId.LISTBOX,
    inherits: RoleType.LIST,
  },
  [RoleType.DOC_EPIGRAPH]:
      {msgId: 'role_doc_epigraph', inherits: AbstractRole.SPAN},
  [RoleType.DOC_EPILOGUE]:
      {msgId: 'role_doc_epilogue', inherits: AbstractRole.SPAN},
  [RoleType.DOC_ERRATA]:
      {msgId: 'role_doc_errata', inherits: AbstractRole.SPAN},
  [RoleType.DOC_EXAMPLE]:
      {msgId: 'role_doc_example', inherits: AbstractRole.SPAN},
  [RoleType.DOC_FOOTNOTE]: {
    msgId: 'role_doc_footnote',
    earcon: EarconId.LIST_ITEM,
    inherits: AbstractRole.ITEM,
  },
  [RoleType.DOC_FOREWORD]:
      {msgId: 'role_doc_foreword', inherits: AbstractRole.SPAN},
  [RoleType.DOC_GLOSSARY]:
      {msgId: 'role_doc_glossary', inherits: AbstractRole.SPAN},
  [RoleType.DOC_GLOSS_REF]: {
    msgId: 'role_doc_gloss_ref',
    earcon: EarconId.LINK,
    inherits: RoleType.LINK,
  },
  [RoleType.DOC_INDEX]: {msgId: 'role_doc_index', inherits: AbstractRole.SPAN},
  [RoleType.DOC_INTRODUCTION]:
      {msgId: 'role_doc_introduction', inherits: AbstractRole.SPAN},
  [RoleType.DOC_NOTE_REF]: {
    msgId: 'role_doc_note_ref',
    earcon: EarconId.LINK,
    inherits: RoleType.LINK,
  },
  [RoleType.DOC_NOTICE]:
      {msgId: 'role_doc_notice', inherits: AbstractRole.SPAN},
  [RoleType.DOC_PAGE_BREAK]:
      {msgId: 'role_doc_page_break', inherits: AbstractRole.SPAN},
  [RoleType.DOC_PAGE_FOOTER]:
      {msgId: 'role_doc_page_footer', inherits: AbstractRole.SPAN},
  [RoleType.DOC_PAGE_HEADER]:
      {msgId: 'role_doc_page_header', inherits: AbstractRole.SPAN},
  [RoleType.DOC_PAGE_LIST]:
      {msgId: 'role_doc_page_list', inherits: AbstractRole.SPAN},
  [RoleType.DOC_PART]: {msgId: 'role_doc_part', inherits: AbstractRole.SPAN},
  [RoleType.DOC_PREFACE]:
      {msgId: 'role_doc_preface', inherits: AbstractRole.SPAN},
  [RoleType.DOC_PROLOGUE]:
      {msgId: 'role_doc_prologue', inherits: AbstractRole.SPAN},
  [RoleType.DOC_PULLQUOTE]:
      {msgId: 'role_doc_pullquote', inherits: AbstractRole.SPAN},
  [RoleType.DOC_QNA]: {msgId: 'role_doc_qna', inherits: AbstractRole.SPAN},
  [RoleType.DOC_SUBTITLE]:
      {msgId: 'role_doc_subtitle', inherits: RoleType.HEADING},
  [RoleType.DOC_TIP]: {msgId: 'role_doc_tip', inherits: AbstractRole.SPAN},
  [RoleType.DOC_TOC]: {msgId: 'role_doc_toc', inherits: AbstractRole.SPAN},
  [RoleType.DOCUMENT]:
      {msgId: 'role_document', inherits: AbstractRole.CONTAINER},
  [RoleType.FORM]: {msgId: 'role_form', inherits: AbstractRole.CONTAINER},
  [RoleType.GRAPHICS_DOCUMENT]:
      {msgId: 'role_graphics_document', inherits: AbstractRole.CONTAINER},
  [RoleType.GRAPHICS_OBJECT]:
      {msgId: 'role_graphics_object', inherits: AbstractRole.CONTAINER},
  [RoleType.GRAPHICS_SYMBOL]:
      {msgId: 'role_graphics_symbol', inherits: RoleType.IMAGE},
  [RoleType.GRID]: {msgId: 'role_grid', inherits: RoleType.TABLE},
  [RoleType.GROUP]: {msgId: 'role_group', inherits: AbstractRole.CONTAINER},
  [RoleType.HEADING]: {
    msgId: 'role_heading',
  },
  [RoleType.IMAGE]: {
    msgId: 'role_img',
  },
  [RoleType.IME_CANDIDATE]: {msgId: 'ime_candidate', ignoreAncestry: true},
  [RoleType.INPUT_TIME]:
      {msgId: 'input_type_time', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  [RoleType.LINK]: {msgId: 'role_link', earcon: EarconId.LINK},
  [RoleType.LIST]: {msgId: 'role_list', inherits: AbstractRole.LIST},
  [RoleType.LIST_BOX]: {msgId: 'role_listbox', earcon: EarconId.LISTBOX},
  [RoleType.LIST_BOX_OPTION]:
      {msgId: 'role_listitem', earcon: EarconId.LIST_ITEM},
  [RoleType.LIST_GRID]: {msgId: 'role_list_grid', inherits: RoleType.TABLE},
  [RoleType.LIST_ITEM]: {
    msgId: 'role_listitem',
    earcon: EarconId.LIST_ITEM,
    inherits: AbstractRole.ITEM,
  },
  [RoleType.LOG]:
      {msgId: 'role_log', inherits: AbstractRole.NAME_FROM_CONTENTS},
  [RoleType.MAIN]: {msgId: 'role_main', inherits: AbstractRole.CONTAINER},
  [RoleType.MARK]: {
    msgId: 'role_mark',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.CONTAINER,
  },
  [RoleType.MARQUEE]:
      {msgId: 'role_marquee', inherits: AbstractRole.NAME_FROM_CONTENTS},
  [RoleType.MATH]: {msgId: 'role_math', inherits: AbstractRole.CONTAINER},
  [RoleType.MENU]: {
    msgId: 'role_menu',
    contextOrder: OutputContextOrder.FIRST,
    ignoreAncestry: true,
  },
  [RoleType.MENU_BAR]: {
    msgId: 'role_menubar',
  },
  [RoleType.MENU_ITEM]: {msgId: 'role_menuitem'},
  [RoleType.MENU_ITEM_CHECK_BOX]: {msgId: 'role_menuitemcheckbox'},
  [RoleType.MENU_ITEM_RADIO]: {msgId: 'role_menuitemradio'},
  [RoleType.MENU_LIST_OPTION]: {msgId: 'role_listitem'},
  [RoleType.MENU_LIST_POPUP]: {msgId: 'role_listbox'},
  [RoleType.METER]: {msgId: 'role_meter', inherits: AbstractRole.RANGE},
  [RoleType.NAVIGATION]:
      {msgId: 'role_navigation', inherits: AbstractRole.CONTAINER},
  [RoleType.NOTE]: {msgId: 'role_note', inherits: AbstractRole.CONTAINER},
  [RoleType.PROGRESS_INDICATOR]:
      {msgId: 'role_progress_indicator', inherits: AbstractRole.RANGE},
  [RoleType.POP_UP_BUTTON]: {
    msgId: 'role_button',
    earcon: EarconId.POP_UP_BUTTON,
    inherits: RoleType.COMBO_BOX_MENU_BUTTON,
  },
  [RoleType.RADIO_BUTTON]: {msgId: 'role_radio'},
  [RoleType.RADIO_GROUP]:
      {msgId: 'role_radiogroup', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  [RoleType.REGION]: {msgId: 'role_region', inherits: AbstractRole.CONTAINER},
  [RoleType.ROW]: {msgId: 'role_row'},
  [RoleType.ROW_HEADER]: {msgId: 'role_rowheader', inherits: RoleType.CELL},
  [RoleType.SCROLL_BAR]:
      {msgId: 'role_scrollbar', inherits: AbstractRole.RANGE},
  [RoleType.SECTION]: {msgId: 'role_region', inherits: AbstractRole.CONTAINER},
  [RoleType.SEARCH]: {msgId: 'role_search', inherits: AbstractRole.CONTAINER},
  [RoleType.SEARCH_BOX]: {msgId: 'role_search', earcon: EarconId.EDITABLE_TEXT},
  [RoleType.SLIDER]: {
    msgId: 'role_slider',
    inherits: AbstractRole.RANGE,
    earcon: EarconId.SLIDER,
  },
  [RoleType.SPIN_BUTTON]: {
    msgId: 'role_spinbutton',
    inherits: AbstractRole.RANGE,
    earcon: EarconId.LISTBOX,
  },
  [RoleType.SPLITTER]: {msgId: 'role_separator', inherits: AbstractRole.SPAN},
  [RoleType.STATUS]:
      {msgId: 'role_status', inherits: AbstractRole.NAME_FROM_CONTENTS},
  [RoleType.SUBSCRIPT]: {msgId: 'role_subscript', inherits: AbstractRole.SPAN},
  [RoleType.SUGGESTION]: {
    msgId: 'role_suggestion',
    contextOrder: OutputContextOrder.FIRST_AND_LAST,
    verboseAncestry: true,
    inherits: AbstractRole.SPAN,
  },
  [RoleType.SUPERSCRIPT]:
      {msgId: 'role_superscript', inherits: AbstractRole.SPAN},
  [RoleType.TAB]: {msgId: 'role_tab', inherits: AbstractRole.CONTAINER},
  [RoleType.TAB_LIST]:
      {msgId: 'role_tablist', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  [RoleType.TAB_PANEL]:
      {msgId: 'role_tabpanel', inherits: AbstractRole.CONTAINER},
  [RoleType.TEXT_FIELD]:
      {msgId: 'input_type_text', earcon: EarconId.EDITABLE_TEXT},
  [RoleType.TEXT_FIELD_WITH_COMBO_BOX]:
      {msgId: 'role_combobox', earcon: EarconId.EDITABLE_TEXT},
  [RoleType.TIME]:
      {msgId: 'tag_time', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  [RoleType.TIMER]:
      {msgId: 'role_timer', inherits: AbstractRole.NAME_FROM_CONTENTS},
  [RoleType.TOGGLE_BUTTON]:
      {msgId: 'role_toggle_button', inherits: RoleType.CHECK_BOX},
  [RoleType.TOOLBAR]: {msgId: 'role_toolbar', ignoreAncestry: true},
  [RoleType.TREE]: {msgId: 'role_tree'},
  [RoleType.TREE_ITEM]: {msgId: 'role_treeitem'},
  [RoleType.VIDEO]:
      {msgId: 'tag_video', inherits: AbstractRole.FORM_FIELD_CONTAINER},
  [RoleType.WINDOW]: {ignoreAncestry: true},
};

TestImportManager.exportForTesting(['OutputRoleInfo', OutputRoleInfo]);
