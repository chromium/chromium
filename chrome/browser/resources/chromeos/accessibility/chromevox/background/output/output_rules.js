// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains the rules for output based on type information.
 */
import {OutputEventType} from './output_types.js';

/**
 * @typedef {{
 *   event: string,
 *   role: string,
 *   navigation: (string|undefined),
 *   output: (string|undefined)}}
 */
export let OutputRuleSpecifier;

export class OutputRule {
  /** @param {string} event */
  constructor(event) {
    /** @private {!OutputEventType} */
    this.event_ = this.getEvent_(event);
    /** @private {string|undefined} */
    this.role_;
    /** @private {string|undefined} */
    this.navigation_;
    /** @private {string|undefined} */
    this.output_;
  }

  /**
   * @param {string} event
   * @return {!OutputEventType}
   * @private
   */
  getEvent_(event) {
    if (Object.values(OutputEventType).includes(event)) {
      return /** @type {!OutputEventType} */ (event);
    }
    return OutputEventType.NAVIGATE;
  }

  /** @return {!OutputRuleSpecifier} */
  get specifier() {
    if (this.event_ === undefined || this.role_ === undefined) {
      throw new Error(
          'Cannot have a completed rule without both an event and a role.');
    }
    return /** @type {!OutputRuleSpecifier} */ ({
      event: this.event_,
      role: this.role_,
      navigation: this.navigation_,
      output: this.output_,
    });
  }

  // The following setter functions are a temporary measure.
  // TODO(anastasi): move the logic for determining the below properties into
  // this class.

  /** @param {string|undefined} role */
  set role(role) {
    this.role_ = role;
  }
  /** @param {string|undefined} navigation */
  set navigation(navigation) {
    this.navigation_ = navigation;
  }
  /** @param {string|undefined} output */
  set output(output) {
    this.output_ = output;
  }

  /** @return {!OutputEventType} */
  get event() {
    return this.event_;
  }
  /** @return {string|undefined} */
  get role() {
    return this.role_;
  }
  /** @return {string|undefined} */
  get navigation() {
    return this.navigation_;
  }
  /** @return {string|undefined} */
  get output() {
    return this.output_;
  }
}

/**
 * Rules specifying format of AutomationNodes for output.
 * @type {!Object<Object<Object<string>>>}
 * Please see below for more information on properties.
 * speak: The speech rule for when ChromeVox range lands exactly on the node.
 * braille: The braille rule for when ChromeVox range lands exactly on the node.
 * enter: The rule for when ChromeVox range enters the node's subtree.
 *    Can contain speak and braille properties.
 * leave: The rule for when ChromeVox range exits the node's subtree.
 * startOf: The rule applied for each ancestor diff of a range and its previous
 * leaf range. endOf: The rule applied for each ancestor diff of a range and its
 * next leaf range.
 */
OutputRule.RULES = {
  navigate: {
    'default': {
      speak: `$name $node(activeDescendant) $value $state $restriction $role
          $description`,
      braille: ``,
    },
    abstractContainer: {
      startOf: `$nameFromNode $role $state $description`,
      endOf: `@end_of_container($role)`,
    },
    abstractFormFieldContainer: {
      enter: `$nameFromNode $role $state $description`,
      leave: `@exited_container($role)`,
    },
    abstractItem: {
      // Note that ChromeVox generally does not output position/count. Only for
      // some roles (see sub-output rules) or when explicitly provided by an
      // author (via posInSet), do we include them in the output.
      enter: `$nameFromNode $role $state $restriction $description
          $if($posInSet, @describe_index($posInSet, $setSize))`,
      speak: `$state $nameOrTextContent= $role
          $if($posInSet, @describe_index($posInSet, $setSize))
          $description $restriction`,
    },
    abstractList: {
      startOf: `$nameFromNode $role @@list_with_items($setSize)
          $restriction $description`,
      endOf: `@end_of_container($role) @@list_nested_level($listNestedLevel)`,
    },
    abstractNameFromContents: {
      speak: `$nameOrDescendants $node(activeDescendant) $value $state
          $restriction $role $description`,
    },
    abstractRange: {
      speak: `$name $node(activeDescendant) $description $role
          $if($value, $value, $if($valueForRange, $valueForRange))
          $state $restriction
          $if($minValueForRange, @aria_value_min($minValueForRange))
          $if($maxValueForRange, @aria_value_max($maxValueForRange))`,
    },
    abstractSpan: {
      startOf: `$nameFromNode $role $state $description`,
      endOf: `@end_of_container($role)`,
    },
    alert: {
      enter: `$name $role $state`,
      speak: `$earcon(ALERT_NONMODAL) $role $nameOrTextContent $description
          $state`,
    },
    alertDialog: {
      enter: `$earcon(ALERT_MODAL) $name $state $description $roleDescription
          $textContent`,
      speak: `$earcon(ALERT_MODAL) $name $nameOrTextContent $description $state
          $role`,
    },
    button: {
      speak: `$name $node(activeDescendant) $state $restriction $role
          $description`,
    },
    cell: {
      enter: {
        speak: `$cellIndexText $node(tableCellColumnHeaders) $nameFromNode
            $roleDescription $state`,
        braille: `$state $cellIndexText $node(tableCellColumnHeaders)
            $nameFromNode $roleDescription`,
      },
      speak: `$nameFromNode $descendants $cellIndexText
          $node(tableCellColumnHeaders) $roleDescription $state $description`,
      braille: `$state
          $name $cellIndexText $node(tableCellColumnHeaders) $roleDescription
          $description
          $if($selected, @aria_selected_true)`,
    },
    checkBox: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $if($checkedStateDescription, $checkedStateDescription, $checked)
          $description $state $restriction`,
    },
    client: {speak: `$name`},
    comboBoxMenuButton: {
      speak: `$name $value $role @aria_has_popup
          $if($setSize, @@list_with_items($setSize))
          $state $restriction $description`,
    },
    date: {enter: `$nameFromNode $role $state $restriction $description`},
    dialog: {enter: `$nameFromNode $role $description`},
    genericContainer: {
      enter: `$nameFromNode $description $state`,
      speak: `$nameOrTextContent $description $state`,
    },
    embeddedObject: {speak: `$name`},
    grid: {
      speak: `$name $node(activeDescendant) $role $state $restriction
          $description`,
    },
    group: {
      enter: `$nameFromNode $roleDescription $state $restriction $description`,
      speak: `$nameOrDescendants $value $state $restriction $roleDescription
          $description`,
      leave: ``,
    },
    heading: {
      enter: `!relativePitch(hierarchicalLevel)
          $nameFromNode=
          $if($hierarchicalLevel, @tag_h+$hierarchicalLevel, $role) $state
          $description`,
      speak: `!relativePitch(hierarchicalLevel)
          $nameOrDescendants=
          $if($hierarchicalLevel, @tag_h+$hierarchicalLevel, $role) $state
          $restriction $description`,
    },
    image: {
      speak: `$if($name, $name,
          $if($imageAnnotation, $imageAnnotation, $urlFilename))
          $value $state $role $description`,
    },
    imeCandidate:
        {speak: '$name $phoneticReading @describe_index($posInSet, $setSize)'},
    inlineTextBox: {speak: `$precedingBullet $name=`},
    inputTime: {enter: `$nameFromNode $role $state $restriction $description`},
    labelText: {
      speak: `$name $value $state $restriction $roleDescription $description`,
    },
    lineBreak: {speak: `$name=`},
    link: {
      enter: `$nameFromNode= $role $state $restriction`,
      speak: `$name $value $state $restriction
          $if($inPageLinkTarget, @internal_link, $role) $description`,
    },
    list: {
      speak: `$nameFromNode $descendants $role
          @@list_with_items($setSize) $description $state`,
    },
    listBox: {
      enter: `$nameFromNode $role @@list_with_items($setSize)
          $restriction $description`,
    },
    listBoxOption: {
      speak: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $nif($selected, @aria_selected_false)`,
      braille: `$state $name $role @describe_index($posInSet, $setSize)
          $description $restriction
          $if($selected, @aria_selected_true, @aria_selected_false)`,
    },
    listMarker: {speak: `$name`},
    menu: {
      enter: `$name $role `,
      speak: `$name $node(activeDescendant)
          $role @@list_with_items($setSize) $description $state $restriction`,
    },
    menuItem: {
      speak: `$name $role $if($hasPopup, @has_submenu)
          @describe_index($posInSet, $setSize) $description $state $restriction`,
    },
    menuItemCheckBox: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $checked $state $restriction $description
          @describe_index($posInSet, $setSize)`,
    },
    menuItemRadio: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_menu_item_radio_selected($name),
          @describe_menu_item_radio_unselected($name)) $state $roleDescription
          $restriction $description
          @describe_index($posInSet, $setSize)`,
    },
    menuListOption: {
      speak: `$name $role @describe_index($posInSet, $setSize) $state
          $nif($selected, @aria_selected_false)
          $restriction $description`,
      braille: `$name $role @describe_index($posInSet, $setSize) $state
          $if($selected, @aria_selected_true, @aria_selected_false)
          $restriction $description`,
    },
    paragraph: {speak: `$nameOrDescendants $roleDescription`},
    radioButton: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_radio_selected($name),
          @describe_radio_unselected($name))
          @describe_index($posInSet, $setSize)
          $roleDescription $description $state $restriction`,
    },
    rootWebArea: {enter: `$name`, speak: `$if($name, $name, @web_content)`},
    region: {speak: `$state $nameOrTextContent $description $roleDescription`},
    row: {
      startOf: `$node(tableRowHeader) $roleDescription
          $if($hierarchicalLevel, @describe_depth($hierarchicalLevel))`,
      speak: ` $if($hierarchicalLevel, @describe_depth($hierarchicalLevel))
          $name $node(activeDescendant) $value $state $restriction $role
          $if($selected, @aria_selected_true) $description`,
    },
    staticText: {speak: `$precedingBullet $name= $description`},
    switch: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $if($checked, @describe_switch_on($name),
          @describe_switch_off($name)) $roleDescription
          $description $state $restriction`,
    },
    tab: {
      speak: `@describe_tab($name) $roleDescription $description
          @describe_index($posInSet, $setSize) $state $restriction
          $if($selected, @aria_selected_true)`,
    },
    table: {
      enter: `$roleDescription @table_summary($name,
          $if($ariaRowCount, $ariaRowCount, $tableRowCount),
          $if($ariaColumnCount, $ariaColumnCount, $tableColumnCount))
          $node(tableHeader)`,
    },
    tabList: {
      speak: `$name $node(activeDescendant) $state $restriction $role
          $description`,
    },
    textField: {
      speak: `$name $value
          $if($roleDescription, $roleDescription,
              $if($multiline, @tag_textarea,
                  $if($inputType, $inputType, $role)))
          $description $state $restriction`,
    },
    timer: {
      speak: `$nameFromNode $descendants $value $state $role
        $description`,
    },
    toggleButton: {
      speak: `$if($checked, $earcon(CHECK_ON), $earcon(CHECK_OFF))
          $name $role $pressed $description $state $restriction`,
    },
    toolbar: {enter: `$name $role $description $restriction`},
    tree: {enter: `$name $role @@list_with_items($setSize) $restriction`},
    treeItem: {
      enter: `$role $expanded $collapsed $restriction
          @describe_index($posInSet, $setSize)
          @describe_depth($hierarchicalLevel)`,
      speak: `$name
          $role $description $state $restriction
          $nif($selected, @aria_selected_false)
          @describe_index($posInSet, $setSize)
          @describe_depth($hierarchicalLevel)`,
    },
    unknown: {speak: ``},
    window: {
      enter: `@describe_window($name) $description`,
      speak: `@describe_window($name) $description $earcon(OBJECT_OPEN)`,
    },
  },
  menuStart:
      {'default': {speak: `@chrome_menu_opened($name)  $earcon(OBJECT_OPEN)`}},
  menuEnd: {'default': {speak: `@chrome_menu_closed $earcon(OBJECT_CLOSE)`}},
  menuListValueChanged: {
    'default': {
      speak: `$value $name
          $find({"state": {"selected": true, "invisible": false}},
          @describe_index($posInSet, $setSize)) `,
    },
  },
  alert: {
    default: {speak: `$earcon(ALERT_NONMODAL) $nameOrTextContent $description`},
  },
};
