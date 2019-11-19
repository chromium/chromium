// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('omnibox_output', function() {
  /**
   * @typedef  {{
   *   cursorPosition: number,
   *   time: number,
   *   done: boolean,
   *   type: string,
   *   host: string,
   *   isTypedHost: boolean,
   * }}
   */
  let ResultsDetails;

  /** @param {!Element} element*/
  function clearChildren(element) {
    while (element.firstChild) {
      element.firstChild.remove();
    }
  }

  class OmniboxOutput extends OmniboxElement {
    constructor() {
      super('omnibox-output-template');

      /** @private {number} */
      this.selectedResponseIndex_ = 0;
      /** @type {!Array<!Array<!mojom.OmniboxResponse>>} */
      this.responsesHistory = [];
      /** @private {!Array<!OutputResultsGroup>} */
      this.resultsGroups_ = [];
      /** @private {!DisplayInputs} */
      this.displayInputs_ = OmniboxInput.defaultDisplayInputs;
      /** @private {string} */
      this.filterText_ = '';
    }

    /** @param {!DisplayInputs} displayInputs */
    updateDisplayInputs(displayInputs) {
      this.displayInputs_ = displayInputs;
      this.updateDisplay_();
    }

    /** @param {string} filterText */
    updateFilterText(filterText) {
      this.filterText_ = filterText;
      this.updateFilterHighlights_();
    }

    /** @param {!Array<!Array<!mojom.OmniboxResponse>>} responsesHistory */
    setResponsesHistory(responsesHistory) {
      this.responsesHistory = responsesHistory;
      this.dispatchEvent(new CustomEvent(
          'responses-count-changed', {detail: responsesHistory.length}));
      this.updateSelectedResponseIndex(this.selectedResponseIndex_);
    }

    /** @param {number} selection */
    updateSelectedResponseIndex(selection) {
      if (selection >= 0 && selection < this.responsesHistory.length) {
        this.selectedResponseIndex_ = selection;
        this.clearResultsGroups_();
        this.responsesHistory[selection].forEach(
            this.createResultsGroup_.bind(this));
      }
    }

    prepareNewQuery() {
      this.responsesHistory.push([]);
      this.dispatchEvent(new CustomEvent(
          'responses-count-changed', {detail: this.responsesHistory.length}));
    }

    /** @param {!mojom.OmniboxResponse} response */
    addAutocompleteResponse(response) {
      const lastIndex = this.responsesHistory.length - 1;
      this.responsesHistory[lastIndex].push(response);
      if (lastIndex === this.selectedResponseIndex_) {
        this.createResultsGroup_(response);
      }
    }

    /**
     * Clears result groups from the UI.
     * @private
     */
    clearResultsGroups_() {
      this.resultsGroups_ = [];
      clearChildren(this.$$('#contents'));
    }

    /**
     * Creates and adds a result group to the UI.
     * @private @param {!mojom.OmniboxResponse} response
     */
    createResultsGroup_(response) {
      const resultsGroup = OutputResultsGroup.create(response);
      this.resultsGroups_.push(resultsGroup);
      this.$$('#contents').appendChild(resultsGroup);

      this.updateDisplay_();
      this.updateFilterHighlights_();
    }

    /**
     * @param {string} url
     * @param {string} data
     */
    updateAnswerImage(url, data) {
      this.autocompleteMatches.forEach(
          match => match.updateAnswerImage(url, data));
    }

    /** @private */
    updateDisplay_() {
      this.updateVisibility_();
      this.updateEliding_();
      this.updateRowHeights_();
    }

    /**
     * Show or hide various output elements depending on display inputs.
     * 1) Show non-last result groups only if showIncompleteResults is true.
     * 2) Show the details section above each table if showDetails or
     * showIncompleteResults are true.
     * 3) Show individual results when showAllProviders is true.
     * 4) Show certain columns and headers only if they showDetails is true.
     * @private
     */
    updateVisibility_() {
      // Show non-last result groups only if showIncompleteResults is true.
      this.resultsGroups_.forEach((resultsGroup, index) => {
        resultsGroup.hidden = !this.displayInputs_.showIncompleteResults &&
            index !== this.resultsGroups_.length - 1;
      });

      this.resultsGroups_.forEach(resultsGroup => {
        resultsGroup.updateVisibility(
            this.displayInputs_.showIncompleteResults,
            this.displayInputs_.showDetails,
            this.displayInputs_.showAllProviders);
      });
    }

    /** @private */
    updateEliding_() {
      this.resultsGroups_.forEach(
          resultsGroup =>
              resultsGroup.updateEliding(this.displayInputs_.elideCells));
    }

    /** @private */
    updateRowHeights_() {
      this.resultsGroups_.forEach(
          resultsGroup =>
              resultsGroup.updateRowHeights(this.displayInputs_.thinRows));
    }

    /** @private */
    updateFilterHighlights_() {
      this.autocompleteMatches.forEach(match => match.filter(this.filterText_));
    }

    /** @return {!Array<!OutputMatch>} */
    get autocompleteMatches() {
      return this.resultsGroups_.flatMap(
          resultsGroup => resultsGroup.autocompleteMatches);
    }
  }

  /**
   * Helps track and render a results group. C++ Autocomplete typically returns
   * 3 result groups per query. It may return less if the next query is
   * submitted before all 3 have been returned. Each result group contains
   * top level information (e.g., how long the result took to generate), as well
   * as a single list of combined results and multiple lists of individual
   * results. Each of these lists is tracked and rendered by OutputResultsTable
   * below.
   */
  class OutputResultsGroup extends OmniboxElement {
    /**
     * @param {!mojom.OmniboxResponse} resultsGroup
     * @return {!OutputResultsGroup}
     */
    static create(resultsGroup) {
      const outputResultsGroup = new OutputResultsGroup();
      outputResultsGroup.setResultsGroup(resultsGroup);
      return outputResultsGroup;
    }

    constructor() {
      super('output-results-group-template');
    }

    /** @param {!mojom.OmniboxResponse} resultsGroup */
    setResultsGroup(resultsGroup) {
      /** @private {ResultsDetails} */
      this.details_ = {
        cursorPosition: resultsGroup.cursorPosition,
        time: resultsGroup.timeSinceOmniboxStartedMs,
        done: resultsGroup.done,
        type: resultsGroup.type,
        host: resultsGroup.host,
        isTypedHost: resultsGroup.isTypedHost,
      };
      /** @type {!Array<!OutputHeader>} */
      this.headers = COLUMNS.map(OutputHeader.create);
      /** @type {!OutputResultsTable} */
      this.combinedResults =
          OutputResultsTable.create(resultsGroup.combinedResults);
      /** @type {!Array<!OutputResultsTable>} */
      this.individualResultsList =
          resultsGroup.resultsByProvider
              .map(resultsWrapper => resultsWrapper.results)
              .filter(results => results.length > 0)
              .map(OutputResultsTable.create);
      if (this.hasAdditionalProperties) {
        this.headers.push(OutputHeader.create(ADDITIONAL_PROPERTIES_COLUMN));
      }
      this.render_();
    }

    /**
     * Creates a HTML Node representing this data.
     * @private
     */
    render_() {
      clearChildren(this);

      /** @private {!Array<!Element>} */
      this.innerHeaders_ = [];

      customElements.whenDefined(this.$$('output-results-details').localName)
          .then(
              () =>
                  this.$$('output-results-details').setDetails(this.details_));

      this.$$('#table').appendChild(this.renderHeader_());
      this.$$('#table').appendChild(this.combinedResults);
      this.individualResultsList.forEach(results => {
        const innerHeader = this.renderInnerHeader_(results);
        this.innerHeaders_.push(innerHeader);
        this.$$('#table').appendChild(innerHeader);
        this.$$('#table').appendChild(results);
      });
    }

    /** @private @return {!Element} */
    renderHeader_() {
      const head = document.createElement('thead');
      head.classList.add('head');
      const row = document.createElement('tr');
      this.headers.forEach(cell => row.appendChild(cell));
      head.appendChild(row);
      return head;
    }

    /**
     * @private
     * @param {!OutputResultsTable} results
     * @return {!Element}
     */
    renderInnerHeader_(results) {
      const head = document.createElement('tbody');
      head.classList.add('head');
      const row = document.createElement('tr');
      const cell = document.createElement('th');
      // Reserve 1 more column for showing the additional properties column.
      cell.colSpan = COLUMNS.length + 1;
      cell.textContent = results.innerHeaderText;
      row.appendChild(cell);
      head.appendChild(row);
      return head;
    }

    /**
     * @param {boolean} showIncompleteResults
     * @param {boolean} showDetails
     * @param {boolean} showAllProviders
     */
    updateVisibility(showIncompleteResults, showDetails, showAllProviders) {
      // Show the details section above each table if showDetails or
      // showIncompleteResults are true.
      this.$$('output-results-details').hidden =
          !showDetails && !showIncompleteResults;

      // Show individual results when showAllProviders is true.
      this.individualResultsList.forEach(
          individualResults => individualResults.hidden = !showAllProviders);
      this.innerHeaders_.forEach(
          innerHeader => innerHeader.hidden = !showAllProviders);

      // Show certain column headers only if they showDetails is true.
      COLUMNS.forEach(({displayAlways}, index) => {
        this.headers[index].hidden = !showDetails && !displayAlways;
      });

      // Show certain columns only if they showDetails is true.
      this.autocompleteMatches.forEach(
          match => match.updateVisibility(showDetails));
    }

    /** @param {boolean} elideCells */
    updateEliding(elideCells) {
      this.autocompleteMatches.forEach(
          match => match.updateEliding(elideCells));
    }

    /** @param {boolean} thinRows */
    updateRowHeights(thinRows) {
      this.autocompleteMatches.forEach(
          match => match.classList.toggle('thin', thinRows));
    }

    /**
     * @private
     * @return {boolean}
     */
    get hasAdditionalProperties() {
      return this.combinedResults.hasAdditionalProperties ||
          this.individualResultsList.some(
              results => results.hasAdditionalProperties);
    }

    /** @return {!Array<!OutputMatch>} */
    get autocompleteMatches() {
      return [this.combinedResults]
          .concat(this.individualResultsList)
          .flatMap(results => results.autocompleteMatches);
    }
  }

  class OutputResultsDetails extends OmniboxElement {
    constructor() {
      super('output-results-details-template');
    }

    /** @param {ResultsDetails} details */
    setDetails(details) {
      this.$$('#cursor-position').textContent = details.cursorPosition;
      this.$$('#time').textContent = details.time;
      this.$$('#done').textContent = details.done;
      this.$$('#type').textContent = details.type;
      this.$$('#host').textContent = details.host;
      this.$$('#is-typed-host').textContent = details.isTypedHost;
    }
  }

  /**
   * Helps track and render a list of results. Each result is tracked and
   * rendered by OutputMatch below.
   */
  class OutputResultsTable extends HTMLTableSectionElement {
    /**
     * @param {!Array<!mojom.AutocompleteMatch>} results
     * @return {!OutputResultsTable}
     */
    static create(results) {
      const resultsTable = new OutputResultsTable();
      resultsTable.results = results;
      return resultsTable;
    }

    constructor() {
      super();
      this.classList.add('body');
      /** @type {!Array<!OutputMatch>} */
      this.autocompleteMatches = [];
    }

    /** @param {!Array<!mojom.AutocompleteMatch>} results */
    set results(results) {
      this.autocompleteMatches.forEach(match => match.remove());
      this.autocompleteMatches = results.map(OutputMatch.create);
      this.autocompleteMatches.forEach(this.appendChild.bind(this));
    }

    /** @return {?string} */
    get innerHeaderText() {
      return this.autocompleteMatches[0].providerName;
    }

    /** @return {boolean} */
    get hasAdditionalProperties() {
      return this.autocompleteMatches.some(
          match => match.hasAdditionalProperties);
    }
  }

  /** Helps track and render a single match. */
  class OutputMatch extends HTMLTableRowElement {
    /**
     * @param {!mojom.AutocompleteMatch} match
     * @return {!OutputMatch}
     */
    static create(match) {
      /** @suppress {checkTypes} */
      const outputMatch = new OutputMatch();
      outputMatch.match = match;
      return outputMatch;
    }

    /** @param {!mojom.AutocompleteMatch} match */
    set match(match) {
      /** @type {!Object<string, !OutputProperty>} */
      this.properties = {};
      /** @type {!OutputProperty} */
      this.properties.contentsAndDescription;
      /** @type {?string} */
      this.providerName = match.providerName || null;

      COLUMNS.forEach(column => {
        const values = column.sourceProperties.map(
            propertyName => /** @type {Object} */ (match)[propertyName]);
        this.properties[column.matchKey] =
            OutputProperty.create(column, values);
      });

      const unconsumedProperties = {};
      Object.entries(match)
          .filter(([name]) => !CONSUMED_SOURCE_PROPERTIES.includes(name))
          .forEach(([name, value]) => unconsumedProperties[name] = value);

      /** @type {!OutputProperty} */
      this.additionalProperties = OutputProperty.create(
          ADDITIONAL_PROPERTIES_COLUMN, [unconsumedProperties]);

      this.render_();
    }

    /** @private */
    render_() {
      clearChildren(this);
      COLUMNS.map(column => this.properties[column.matchKey])
          .forEach(cell => this.appendChild(cell));
      if (this.hasAdditionalProperties) {
        this.appendChild(this.additionalProperties);
      }
    }

    /**
     * @param {string} url
     * @param {string} data
     */
    updateAnswerImage(url, data) {
      if (this.properties.contentsAndDescription.value === url) {
        this.properties.contentsAndDescription.setAnswerImageData(data);
      }
    }

    /** @param {boolean} showDetails */
    updateVisibility(showDetails) {
      // Show certain columns only if they showDetails is true.
      COLUMNS.forEach(({matchKey, displayAlways}) => {
        this.properties[matchKey].hidden = !showDetails && !displayAlways;
      });
    }

    /** @param {boolean} elideCells */
    updateEliding(elideCells) {
      Object.values(this.properties)
          .forEach(property => property.classList.toggle('elided', elideCells));
    }

    /** @param {string} filterText */
    filter(filterText) {
      this.classList.remove('filtered-highlighted');
      this.allProperties_.forEach(
          property => property.classList.remove('filtered-highlighted-nested'));

      if (!filterText) {
        return;
      }

      const matchedProperties = this.allProperties_.filter(
          property => FilterUtil.filterText(property.text, filterText));
      const isMatch = matchedProperties.length > 0;
      this.classList.toggle('filtered-highlighted', isMatch);
      matchedProperties.forEach(
          property => property.classList.add('filtered-highlighted-nested'));
    }

    /**
     * @return {boolean} Used to determine if the additional properties column
     * needs to be displayed for this match.
     */
    get hasAdditionalProperties() {
      return Object
                 .keys(/** @type {!Object} */ (this.additionalProperties.value))
                 .length > 0;
    }

    /** @private @return {!Array<!OutputProperty>} */
    get allProperties_() {
      return Object.values(this.properties).concat(this.additionalProperties);
    }
  }

  class OutputHeader extends HTMLTableCellElement {
    /**
     * @param {Column} column
     * @return {!OutputHeader}
     */
    static create(column) {
      const header = new OutputHeader();
      header.classList.add(column.headerClassName);
      header.setContents(column.headerText, column.url);
      header.title = column.tooltip;
      return header;
    }

    /**
     * @param {!Array<string>} texts
     * @param {string=} url
     */
    setContents(texts, url) {
      clearChildren(this);
      let container;
      if (url) {
        container = document.createElement('a');
        container.href = url;
      } else {
        container = document.createElement('div');
      }
      container.classList.add('header-container');
      texts.forEach(text => {
        const part = document.createElement('span');
        part.textContent = text;
        container.appendChild(part);
      });
      this.appendChild(container);
    }
  }

  class OutputProperty extends HTMLTableCellElement {
    constructor() {
      super();
      /** @type {string} */
      this.filterName;
    }

    /**
     * @param {Column} column
     * @param {!Array<*>} values
     * @return {!OutputProperty}
     */
    static create(column, values) {
      const outputProperty = new column.outputClass();
      outputProperty.classList.add(column.cellClassName);
      outputProperty.filterName = column.tooltip.split('\n', 1)[0];
      outputProperty.values = values;
      return outputProperty;
    }

    /** @param {!Array<*>} values */
    set values(values) {
      /** @type {*} */
      this.value = values[0];
      /** @private {!Array<*>} */
      this.values_ = values;
      /** @override */
      this.render_();
    }

    /** @private */
    render_() {}

    /** @return {string} */
    get text() {
      return this.value + '';
    }
  }

  class FlexWrappingOutputProperty extends OutputProperty {
    constructor() {
      super();

      // margin-right is used on .pair-item's to separate them. To compensate,
      // .pair-container has negative margin-right. This means .pair-container's
      // overflow their parent. Overflowing a table cell is problematic, as 1)
      // scroll bars overlay adjacent cell, and 2) the page receives a
      // horizontal scroll bar when the right most column overflows. To avoid
      // this, the parent of any element with negative margins (e.g.
      // .pair-container) must not be a table cell; hence, the use of
      // scrollContainer_.
      // Flex gutters may provide a cleaner alternative once implemented.
      // https://developer.mozilla.org/en-US/docs/Web/CSS/CSS_Flexible_Box_Layout/Mastering_Wrapping_of_Flex_Items#Creating_gutters_between_items
      /** @private {!Element} */
      this.scrollContainer_ = document.createElement('div');
      this.appendChild(this.scrollContainer_);

      /** @private {!Element} */
      this.container_ = document.createElement('div');
      this.container_.classList.add('pair-container');
      this.scrollContainer_.appendChild(this.container_);
    }
  }

  class OutputPairProperty extends FlexWrappingOutputProperty {
    constructor() {
      super();

      /** @type {!Element} */
      this.first_ = document.createElement('div');
      this.first_.classList.add('pair-item');
      this.container_.appendChild(this.first_);

      /** @type {!Element} */
      this.second_ = document.createElement('div');
      this.second_.classList.add('pair-item');
      this.container_.appendChild(this.second_);
    }

    /** @private @override */
    render_() {
      [this.first_.textContent, this.second_.textContent] = this.values_;
    }

    /** @override @return {string} */
    get text() {
      return `${this.values_[0]}.${this.values_[1]}`;
    }
  }

  class OutputOverlappingPairProperty extends OutputPairProperty {
    constructor() {
      super();

      this.notOverlapWarning_ = document.createElement('div');
      this.notOverlapWarning_.classList.add('overlap-warning');
      this.container_.appendChild(this.notOverlapWarning_);
    }

    /** @private @override */
    render_() {
      const overlap = this.values_[0].endsWith(this.values_[1]);
      const firstText = this.values_[1] && overlap ?
          this.values_[0].slice(0, -this.values_[1].length) :
          this.values_[0];

      this.first_.textContent = firstText;
      this.second_.textContent = this.values_[1];
      this.notOverlapWarning_.textContent = overlap ?
          '' :
          `btw, these texts do not overlap; '${
              this.values_[1]}' was expected to be a suffix of '${
              this.values_[0]}'`;
    }
  }

  class OutputAnswerProperty extends FlexWrappingOutputProperty {
    constructor() {
      super();

      /** @type {!Element} */
      this.image_ = document.createElement('img');
      this.image_.classList.add('pair-item');
      this.container_.appendChild(this.image_);

      /** @type {!Element} */
      this.contents_ = document.createElement('div');
      this.contents_.classList.add('pair-item', 'contents');
      this.container_.appendChild(this.contents_);

      /** @type {!Element} */
      this.description_ = document.createElement('div');
      this.description_.classList.add('pair-item', 'description');
      this.container_.appendChild(this.description_);

      /** @type {!Element} */
      this.answer_ = document.createElement('div');
      this.answer_.classList.add('pair-item', 'answer');
      this.container_.appendChild(this.answer_);

      /** @type {!Element} */
      this.imageUrl_ = document.createElement('a');
      this.imageUrl_.classList.add('pair-item', 'image-url');
      this.container_.appendChild(this.imageUrl_);
    }

    /** @param {string} imageData */
    setAnswerImageData(imageData) {
      this.image_.src = imageData;
    }

    /** @private @override */
    render_() {
      // TODO (manukh) Wrap this line when Clang is updated,
      // https://b.corp.google.com/126708256 .
      const [image, contents, description, answer, contentsClassification, descriptionClassification] =
          this.values_;
      OutputAnswerProperty.renderClassifiedText_(
          this.contents_, /** @type {string} */ (contents),
          /** @type {!Array<!mojom.ACMatchClassification>} */
          (contentsClassification));
      OutputAnswerProperty.renderClassifiedText_(
          this.description_, /** @type {string} */ (description),
          /** @type {!Array<!mojom.ACMatchClassification>} */
          (descriptionClassification));
      this.answer_.textContent = answer;
      this.imageUrl_.textContent = image;
      this.imageUrl_.href = image;
    }

    /** @override @return {string} */
    get text() {
      return this.values_.join('.');
    }

    /**
     * @private
     * @param {!Element} container
     * @param {string} string
     * @param {!Array<!mojom.ACMatchClassification>} classes
     */
    static renderClassifiedText_(container, string, classes) {
      clearChildren(container);
      OutputAnswerProperty.classify(string, classes)
          .map(
              ({string, style}) => OutputJsonProperty.renderJsonWord(
                  string, OutputAnswerProperty.styleToClasses_(style)))
          .forEach(span => container.appendChild(span));
    }

    /**
     * @param {string} string
     * @param {!Array<!mojom.ACMatchClassification>} classes
     * @return {!Array<{string: string, style: number}>}
     */
    static classify(string, classes) {
      return classes.map(({offset, style}, i) => {
        const end = classes[i + 1] ? classes[i + 1].offset : string.length;
        return {string: string.substring(offset, end), style};
      });
    }

    /**
     * @private
     * @param {number} style
     * @return {!Array<string>}
     */
    static styleToClasses_(style) {
      // Maps the bitmask enum AutocompleteMatch::ACMatchClassification::Style
      // to strings. See autocomplete_match.h for more details.
      // E.g., maps the style 5 to classes ['style-url', 'style-dim'].
      return ['style-url', 'style-match', 'style-dim'].filter(
          (_, i) => (style >> i) % 2);
    }
  }

  class OutputBooleanProperty extends OutputProperty {
    constructor() {
      super();
      /** @private {!Element} */
      this.icon_ = document.createElement('div');
      this.appendChild(this.icon_);
    }

    /** @private @override */
    render_() {
      this.icon_.classList.toggle('check-mark', !!this.value);
      this.icon_.classList.toggle('x-mark', !this.value);
    }

    get text() {
      return (this.value ? 'is: ' : 'not: ') + this.filterName;
    }
  }

  class OutputJsonProperty extends OutputProperty {
    constructor() {
      super();
      /** @private {!Element} */
      this.pre_ = document.createElement('pre');
      this.pre_.classList.add('json');
      this.appendChild(this.pre_);
    }

    /** @private @override */
    render_() {
      clearChildren(this.pre_);
      this.text.split(/("(?:[^"\\]|\\.)*":?|\w+)/)
          .map(word => {
            return OutputJsonProperty.renderJsonWord(
                word, [OutputJsonProperty.classifyJsonWord(word)]);
          })
          .forEach(jsonSpan => this.pre_.appendChild(jsonSpan));
    }

    /** @override @return {string} */
    get text() {
      return JSON.stringify(this.value, null, 2);
    }

    /**
     * @param {string} word
     * @param {!Array<string>} classes
     * @return {!Element}
     */
    static renderJsonWord(word, classes) {
      const span = document.createElement('span');
      span.classList.add(...classes);
      span.textContent = word;
      return span;
    }

    /**
     * @param {string} word
     * @return {string|undefined}
     */
    static classifyJsonWord(word) {
      // Statically creating the regexes only once.
      OutputJsonProperty.classifications =
          OutputJsonProperty.classifications || [
            {re: /^"[^]*":$/, clazz: 'key'},
            {re: /^"[^]*"$/, clazz: 'string'},
            {re: /true|false/, clazz: 'boolean'},
            {re: /null/, clazz: 'null'},
          ];
      OutputJsonProperty.spaceRegex = OutputJsonProperty.spaceRegex || /^\s*$/;

      // Using isNaN, because Number.isNaN checks explicitly for NaN whereas
      // isNaN coerces the param to a Number. I.e. isNaN('3') === false, while
      // Number.isNaN('3') === true.
      if (isNaN(word)) {
        const classification =
            OutputJsonProperty.classifications.find(({re}) => re.test(word));
        return classification && classification.clazz;
      } else if (!OutputJsonProperty.spaceRegex.test(word)) {
        return 'number';
      }
    }
  }

  class OutputAdditionalInfoProperty extends OutputProperty {
    constructor() {
      super();
      const container = document.createElement('div');

      /** @private {!Element} */
      this.pre_ = document.createElement('pre');
      this.pre_.classList.add('json');
      container.appendChild(this.pre_);

      /** @private {!Element} */
      this.link_ = document.createElement('a');
      this.link_.download = 'AdditionalInfo.json';

      container.appendChild(this.link_);
      this.appendChild(container);
    }

    /** @private @override */
    render_() {
      clearChildren(this.pre_);
      this.value.forEach(({key, value}) => {
        this.pre_.appendChild(
            OutputJsonProperty.renderJsonWord(key + ': ', ['key']));
        this.pre_.appendChild(
            OutputJsonProperty.renderJsonWord(value + '\n', ['number']));
      });
      this.link_.href = this.createDownloadLink_();
    }

    /** @override @return {string} */
    get text() {
      return this.value.reduce(
          (prev, {key, value}) => `${prev}${key}: ${value}\n`, '');
    }

    /** @private @return {string} */
    createDownloadLink_() {
      const obj = this.value.reduce((obj, {key, value}) => {
        obj[key] = value;
        return obj;
      }, {});
      const obj64 = btoa(unescape(encodeURIComponent(JSON.stringify(obj))));
      return `data:application/json;base64,${obj64}`;
    }
  }

  class OutputUrlProperty extends FlexWrappingOutputProperty {
    constructor() {
      super();

      /** @private {!Element} */
      this.iconAndUrlContainer_ = document.createElement('div');
      this.iconAndUrlContainer_.classList.add('pair-item');
      this.container_.appendChild(this.iconAndUrlContainer_);

      /** @private {!Element} */
      this.icon_ = document.createElement('img');
      this.iconAndUrlContainer_.appendChild(this.icon_);

      /** @private {!Element} */
      this.urlLink_ = document.createElement('a');
      this.iconAndUrlContainer_.appendChild(this.urlLink_);

      /** @private {!Element} */
      this.strippedUrlLink_ = document.createElement('a');
      this.strippedUrlLink_.classList.add('pair-item');
      this.container_.appendChild(this.strippedUrlLink_);
    }

    /** @private @override */
    render_() {
      const [destinationUrl, isSearchType, strippedDestinationUrl] =
          this.values_;
      if (isSearchType) {
        this.icon_.removeAttribute('src');
      } else {
        this.icon_.src = `chrome://favicon/${destinationUrl}`;
      }
      this.urlLink_.textContent = destinationUrl;
      this.urlLink_.href = destinationUrl;
      this.strippedUrlLink_.textContent = strippedDestinationUrl;
      this.strippedUrlLink_.href = strippedDestinationUrl;
    }
  }

  class OutputTextProperty extends OutputProperty {
    constructor() {
      super();
      /** @private {!Element} */
      this.div_ = document.createElement('div');
      this.appendChild(this.div_);
    }

    /** @private @override */
    render_() {
      this.div_.textContent = this.value;
    }
  }

  /** Responsible for highlighting and hiding rows using filter text. */
  class FilterUtil {
    /**
     * Checks if a string fuzzy-matches a filter string. Each character
     * of filterText must be present in the search text, either adjacent to the
     * previous matched character, or at the start of a new word (see
     * textToWords_).
     * E.g. `abc` matches `abc`, `a big cat`, `a-bigCat`, `a very big cat`, and
     * `an amBer cat`; but does not match `abigcat` or `an amber cat`.
     * `green rainbow` is matched by `gre rain`, but not by `gre bow`.
     * One exception is the first character, which may be matched mid-word.
     * E.g. `een rain` can also match `green rainbow`.
     * @param {string} searchText
     * @param {string} filterText
     * @return {boolean}
     */
    static filterText(searchText, filterText) {
      const regexFilter =
          Array.from(filterText)
              .map(word => word.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'))
              .join('(.*\\.)?');
      const words = FilterUtil.textToWords_(searchText).join('.');
      return words.match(new RegExp(regexFilter, 'i')) !== null;
    }

    /**
     * Splits a string into words, delimited by either capital letters, groups
     * of digits, or non alpha characters.
     * E.g., `https://google.com/the-dog-ate-134pies` will be split to:
     * https, :, /, /, google, ., com, /, the, -,  dog, -, ate, -, 134, pies
     * This differs from `Array.split` in that this groups digits, e.g. 134.
     * @private
     * @param {string} text
     * @return {!Array<string>}
     */
    static textToWords_(text) {
      const MAX_TEXT_LENGTH = 200;
      if (text.length > MAX_TEXT_LENGTH) {
        text = text.slice(0, MAX_TEXT_LENGTH);
        console.warn(`text to be filtered too long, truncatd; max length: ${
            MAX_TEXT_LENGTH}, truncated text: ${text}`);
      }
      return text.match(/[a-z]+|[A-Z][a-z]*|\d+|./g) || [];
    }
  }

  class Column {
    /**
     * @param {!Array<string>} headerText
     * @param {string} url
     * @param {string} matchKey
     * @param {boolean} displayAlways
     * @param {string} tooltip
     * @param {function(new:OutputProperty)} outputClass
     * @param {!Array<string>} sourceProperties
     */
    constructor(
        headerText, url, matchKey, displayAlways, tooltip, sourceProperties,
        outputClass) {
      /** @type {!Array<string>} split per span container to support styling. */
      this.headerText = headerText;
      /** @type {string} header link href or blank if non-hyperlink header. */
      this.url = url;
      /** @type {string} the field name used in the Match.properties object. */
      this.matchKey = matchKey;
      /** @type {boolean} if shown when showDetails option is false. */
      this.displayAlways = displayAlways;
      /** @type {string} header tooltip. */
      this.tooltip = tooltip;
      /** @type {!Array<string>} related mojo AutocompleteMatch properties. */
      this.sourceProperties = sourceProperties;
      /** @type {function(new:OutputProperty)} */
      this.outputClass = outputClass;

      const hyphenatedName =
          matchKey.replace(/[A-Z]/g, c => '-' + c.toLowerCase());
      /** @type {string} */
      this.cellClassName = 'cell-' + hyphenatedName;
      /** @type {string} */
      this.headerClassName = 'header-' + hyphenatedName;
    }
  }

  /**
   * A constant that's used to decide what autocomplete result
   * properties to output in what order.
   * @type {!Array<!Column>}
   */
  const COLUMNS = [
    new Column(
        ['Provider', 'Type'], '', 'providerAndType', true,
        'Provider & Type\nThe AutocompleteProvider suggesting this result. / ' +
            'The type of the result.',
        ['providerName', 'type'], OutputPairProperty),
    new Column(
        ['Relevance'], '', 'relevance', true,
        'Relevance\nThe result score. Higher is more relevant.', ['relevance'],
        OutputTextProperty),
    new Column(
        ['Contents', 'Description', 'Answer'], '', 'contentsAndDescription',
        true,
        'Contents & Description & Answer\nURL classifications are styled ' +
            'blue.\nMATCH classifications are styled bold.\nDIM ' +
            'classifications are styled with a gray background.',
        [
          'image', 'contents', 'description', 'answer', 'contentsClass',
          'descriptionClass'
        ],
        OutputAnswerProperty),
    new Column(
        ['S'], '', 'swapContentsAndDescription', false,
        'Swap Contents and Description', ['swapContentsAndDescription'],
        OutputBooleanProperty),
    new Column(
        ['D'], '', 'allowedToBeDefaultMatch', true,
        'Can be Default\nA green checkmark indicates that the result can be ' +
            'the default match (i.e., can be the match that pressing enter ' +
            'in the omnibox navigates to).',
        ['allowedToBeDefaultMatch'], OutputBooleanProperty),
    new Column(
        ['S'], '', 'starred', false,
        'Starred\nA green checkmark indicates that the result has been ' +
            'bookmarked.',
        ['starred'], OutputBooleanProperty),
    new Column(
        ['T'], '', 'hasTabMatch', false,
        'Has Tab Match\nA green checkmark indicates that the result URL ' +
            'matches an open tab.',
        ['hasTabMatch'], OutputBooleanProperty),
    new Column(
        ['URL', 'Stripped URL'], '', 'destinationUrl', true,
        'URL & Stripped URL\nThe URL for the result. / The stripped URL for ' +
            'the result.',
        ['destinationUrl', 'isSearchType', 'strippedDestinationUrl'],
        OutputUrlProperty),
    new Column(
        ['Fill', 'Inline'], '', 'fillAndInline', false,
        'Fill & Inline\nThe text shown in the omnibox when the result is ' +
            'selected. / The text shown in the omnibox as a blue highlight ' +
            'selection following the cursor, if this match is shown inline.',
        ['fillIntoEdit', 'inlineAutocompletion'],
        OutputOverlappingPairProperty),
    new Column(
        ['D'], '', 'deletable', false,
        'Deletable\nA green checkmark indicates that the result can be ' +
            'deleted from the visit history.',
        ['deletable'], OutputBooleanProperty),
    new Column(
        ['P'], '', 'fromPrevious', false,
        'From Previous\nTrue if this match is from a previous result.',
        ['fromPrevious'], OutputBooleanProperty),
    new Column(
        ['Tran'],
        'https://cs.chromium.org/chromium/src/ui/base/page_transition_types.h' +
            '?q=page_transition_types.h&sq=package:chromium&dr=CSs&l=14',
        'transition', false, 'Transition\nHow the user got to the result.',
        ['transition'], OutputTextProperty),
    new Column(
        ['D'], '', 'providerDone', false,
        'Done\nA green checkmark indicates that the provider is done looking ' +
            'for more results.',
        ['providerDone'], OutputBooleanProperty),
    new Column(
        ['Associated Keyword'], '', 'associatedKeyword', false,
        'Associated Keyword\nIf non-empty, a "press tab to search" hint will ' +
            'be shown and will engage this keyword.',
        ['associatedKeyword'], OutputTextProperty),
    new Column(
        ['Keyword'], '', 'keyword', false,
        'Keyword\nThe keyword of the search engine to be used.', ['keyword'],
        OutputTextProperty),
    new Column(
        ['D'], '', 'duplicates', false,
        'Duplicates\nThe number of matches that have been marked as ' +
            'duplicates of this match.',
        ['duplicates'], OutputTextProperty),
    new Column(
        ['Additional Info'], '', 'additionalInfo', false,
        'Additional Info\nProvider-specific information about the result.',
        ['additionalInfo'], OutputAdditionalInfoProperty)
  ];

  /** @type {!Column} */
  const ADDITIONAL_PROPERTIES_COLUMN = new Column(
      ['Additional Properties'], '', 'additionalProperties', false,
      'Properties not accounted for.', [], OutputJsonProperty);

  const CONSUMED_SOURCE_PROPERTIES =
      COLUMNS.flatMap(column => column.sourceProperties);

  customElements.define('omnibox-output', OmniboxOutput);
  customElements.define('output-results-group', OutputResultsGroup);
  customElements.define('output-results-details', OutputResultsDetails);
  customElements.define(
      'output-results-table', OutputResultsTable, {extends: 'tbody'});
  customElements.define('output-match', OutputMatch, {extends: 'tr'});
  customElements.define('output-header', OutputHeader, {extends: 'th'});
  customElements.define(
      'output-pair-property', OutputPairProperty, {extends: 'td'});
  customElements.define(
      'output-overlapping-pair-property', OutputOverlappingPairProperty,
      {extends: 'td'});
  customElements.define(
      'output-answer-property', OutputAnswerProperty, {extends: 'td'});
  customElements.define(
      'output-boolean-property', OutputBooleanProperty, {extends: 'td'});
  customElements.define(
      'output-json-property', OutputJsonProperty, {extends: 'td'});
  customElements.define(
      'output-additional-info-property', OutputAdditionalInfoProperty,
      {extends: 'td'});
  customElements.define(
      'output-url-property', OutputUrlProperty, {extends: 'td'});
  customElements.define(
      'output-text-property', OutputTextProperty, {extends: 'td'});

  return {OmniboxOutput: OmniboxOutput};
});
