// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class which allows construction of annotated strings.
 */

export class Spannable {
  /**
   * @param {string|!Spannable=} opt_string Initial value of the spannable.
   * @param {*=} opt_annotation Initial annotation for the entire string.
   */
  constructor(opt_string, opt_annotation) {
    /**
     * Underlying string.
     * @type {string}
     * @private
     */
    this.string_ = opt_string instanceof Spannable ? '' : opt_string || '';

    /**
     * Spans (annotations).
     * @type {!Array<!SpanStruct>}
     * @private
     */
    this.spans_ = [];

    // Append the initial spannable.
    if (opt_string instanceof Spannable) {
      this.append(opt_string);
    }

    // Optionally annotate the entire string.
    if (goog.isDef(opt_annotation)) {
      const len = this.string_.length;
      this.spans_.push({value: opt_annotation, start: 0, end: len});
    }
  }

  /** @override */
  toString() {
    return this.string_;
  }

  /** @return {number} The length of the string */
  get length() {
    return this.string_.length;
  }

  /**
   * Adds a span to some region of the string.
   * @param {*} value Annotation.
   * @param {number} start Starting index (inclusive).
   * @param {number} end Ending index (exclusive).
   */
  setSpan(value, start, end) {
    this.removeSpan(value);
    this.setSpanInternal(value, start, end);
  }

  /**
   * @param {*} value Annotation.
   * @param {number} start Starting index (inclusive).
   * @param {number} end Ending index (exclusive).
   * @protected
   */
  setSpanInternal(value, start, end) {
    if (0 <= start && start <= end && end <= this.string_.length) {
      // Zero-length spans are explicitly allowed, because it is possible to
      // query for position by annotation as well as the reverse.
      this.spans_.push({value, start, end});
      this.spans_.sort(function(a, b) {
        let ret = a.start - b.start;
        if (ret === 0) {
          ret = a.end - b.end;
        }
        return ret;
      });
    } else {
      throw new RangeError(
          'span out of range (start=' + start + ', end=' + end +
          ', len=' + this.string_.length + ')');
    }
  }

  /**
   * Removes a span.
   * @param {*} value Annotation.
   */
  removeSpan(value) {
    for (let i = this.spans_.length - 1; i >= 0; i--) {
      if (this.spans_[i].value === value) {
        this.spans_.splice(i, 1);
      }
    }
  }

  /**
   * Appends another Spannable or string to this one.
   * @param {string|!Spannable} other String or spannable to concatenate.
   */
  append(other) {
    if (other instanceof Spannable) {
      const otherSpannable = /** @type {!Spannable} */ (other);
      const originalLength = this.length;
      this.string_ += otherSpannable.string_;
      other.spans_.forEach(
          span => this.setSpan(
              span.value, span.start + originalLength,
              span.end + originalLength));
    } else if (typeof other === 'string') {
      this.string_ += /** @type {string} */ (other);
    }
  }

  /**
   * Returns the first value matching a position.
   * @param {number} position Position to query.
   * @return {*} Value annotating that position, or undefined if none is
   *     found.
   */
  getSpan(position) {
    return valueOfSpan(this.spans_.find(spanCoversPosition(position)));
  }

  /**
   * Returns the first span value which is an instance of a given constructor.
   * @param {!Function} constructor Constructor.
   * @return {*} Object if found; undefined otherwise.
   */
  getSpanInstanceOf(constructor) {
    return valueOfSpan(this.spans_.find(spanInstanceOf(constructor)));
  }

  /**
   * Returns all span values which are an instance of a given constructor.
   * Spans are returned in the order of their starting index and ending index
   * for spans with equals tarting indices.
   * @param {!Function} constructor Constructor.
   * @return {!Array<Object>} Array of object.
   */
  getSpansInstanceOf(constructor) {
    return (this.spans_.filter(spanInstanceOf(constructor)).map(valueOfSpan));
  }

  /**
   * Returns all spans matching a position.
   * @param {number} position Position to query.
   * @return {!Array} Values annotating that position.
   */
  getSpans(position) {
    return (this.spans_.filter(spanCoversPosition(position)).map(valueOfSpan));
  }

  /**
   * Returns whether a span is contained in this object.
   * @param {*} value Annotation.
   * @return {boolean}
   */
  hasSpan(value) {
    return this.spans_.some(spanValueIs(value));
  }

  /**
   * Returns the start of the requested span. Throws if the span doesn't exist
   * in this object.
   * @param {*} value Annotation.
   * @return {number}
   */
  getSpanStart(value) {
    return this.getSpanByValueOrThrow_(value).start;
  }

  /**
   * Returns the end of the requested span. Throws if the span doesn't exist
   * in this object.
   * @param {*} value Annotation.
   * @return {number}
   */
  getSpanEnd(value) {
    return this.getSpanByValueOrThrow_(value).end;
  }

  /**
   * @param {*} value Annotation.
   * @return {!Array<{start: number, end: number}>}
   */
  getSpanIntervals(value) {
    return this.spans_.filter(span => span.value === value).map(span => {
      return {start: span.start, end: span.end};
    });
  }

  /**
   * Returns the number of characters covered by the given span. Throws if
   * the span is not in this object.
   * @param {*} value
   * @return {number}
   */
  getSpanLength(value) {
    const span = this.getSpanByValueOrThrow_(value);
    return span.end - span.start;
  }

  /**
   * Gets the internal object for a span or throws if the span doesn't exist.
   * @param {*} value The annotation.
   * @return {!SpanStruct}
   * @private
   */
  getSpanByValueOrThrow_(value) {
    const span = this.spans_.find(spanValueIs(value));
    if (span) {
      return span;
    }
    throw new Error('Span ' + value + ' doesn\'t exist in spannable');
  }

  /**
   * Returns a substring of this spannable.
   * Note that while similar to String#substring, this function is much less
   * permissive about its arguments. It does not accept arguments in the wrong
   * order or out of bounds.
   *
   * @param {number} start Start index, inclusive.
   * @param {number=} opt_end End index, exclusive.
   *     If excluded, the length of the string is used instead.
   * @return {!Spannable} Substring requested.
   */
  substring(start, opt_end) {
    const end = goog.isDef(opt_end) ? opt_end : this.string_.length;

    if (start < 0 || end > this.string_.length || start > end) {
      throw new RangeError('substring indices out of range');
    }

    const result = new Spannable(this.string_.substring(start, end));
    this.spans_.forEach(span => {
      if (span.start <= end && span.end >= start) {
        const newStart = Math.max(0, span.start - start);
        const newEnd = Math.min(end - start, span.end - start);
        result.spans_.push({value: span.value, start: newStart, end: newEnd});
      }
    });
    return result;
  }

  /**
   * Trims whitespace from the beginning.
   * @return {!Spannable} String with whitespace removed.
   */
  trimLeft() {
    return this.trim_(true, false);
  }

  /**
   * Trims whitespace from the end.
   * @return {!Spannable} String with whitespace removed.
   */
  trimRight() {
    return this.trim_(false, true);
  }

  /**
   * Trims whitespace from the beginning and end.
   * @return {!Spannable} String with whitespace removed.
   */
  trim() {
    return this.trim_(true, true);
  }

  /**
   * Trims whitespace from either the beginning and end or both.
   * @param {boolean} trimStart Trims whitespace from the start of a string.
   * @param {boolean} trimEnd Trims whitespace from the end of a string.
   * @return {!Spannable} String with whitespace removed.
   * @private
   */
  trim_(trimStart, trimEnd) {
    if (!trimStart && !trimEnd) {
      return this;
    }

    // Special-case whitespace-only strings, including the empty string.
    // As an arbitrary decision, we treat this as trimming the whitespace off
    // the end, rather than the beginning, of the string.
    // This choice affects which spans are kept.
    if (/^\s*$/.test(this.string_)) {
      return this.substring(0, 0);
    }

    // Otherwise, we have at least one non-whitespace character to use as an
    // anchor when trimming.
    const trimmedStart = trimStart ? this.string_.match(/^\s*/)[0].length : 0;
    const trimmedEnd =
        trimEnd ? this.string_.match(/\s*$/).index : this.string_.length;
    return this.substring(trimmedStart, trimmedEnd);
  }

  /**
   * Returns this spannable to a json serializable form, including the text
   * and span objects whose types have been registered with
   * registerSerializableSpan or registerStatelessSerializableSpan.
   * @return {!SerializedSpannable} the json serializable form.
   */
  toJson() {
    const result = {};
    result.string = this.string_;
    result.spans = [];
    this.spans_.forEach(span => {
      const serializeInfo =
          serializableSpansByConstructor.get(span.value.constructor);
      if (serializeInfo) {
        const spanObj = {
          type: serializeInfo.name,
          start: span.start,
          end: span.end,
        };
        if (serializeInfo.toJson) {
          spanObj.value = serializeInfo.toJson.apply(span.value);
        }
        result.spans.push(spanObj);
      }
    });
    return result;
  }

  /**
   * Creates a spannable from a json serializable representation.
   * @param {!SerializedSpannable} obj object containing the serializable
   *     representation.
   * @return {!Spannable}
   */
  static fromJson(obj) {
    if (typeof obj.string !== 'string') {
      throw new Error(
          'Invalid spannable json object: string field not a string');
    }
    if (!(obj.spans instanceof Array)) {
      throw new Error('Invalid spannable json object: no spans array');
    }
    const result = new Spannable(obj.string);
    result.spans_ = obj.spans.map(span => {
      if (typeof span.type !== 'string') {
        throw new Error(
            'Invalid span in spannable json object: type not a string');
      }
      if (typeof span.start !== 'number' || typeof span.end !== 'number') {
        throw new Error(
            'Invalid span in spannable json object: start or end not a number');
      }
      const serializeInfo = serializableSpansByName.get(span.type);
      const value = serializeInfo.fromJson(span.value);
      return {value, start: span.start, end: span.end};
    });
    return result;
  }

  /**
   * Registers a type that can be converted to a json serializable format.
   * @param {!Function} constructor The type of object that can be converted.
   * @param {string} name String identifier used in the serializable format.
   * @param {function(!Object): !Object} fromJson A function that converts
   *     the serializable object to an actual object of this type.
   * @param {function(): !Object} toJson A function that converts this object
   *     to a json serializable object. The function will be called with
   *     |this| set to the object to convert.
   */
  static registerSerializableSpan(constructor, name, fromJson, toJson) {
    const obj = {name, fromJson, toJson};
    serializableSpansByName.set(name, obj);
    serializableSpansByConstructor.set(constructor, obj);
  }

  /**
   * Registers an object type that can be converted to/from a json
   * serializable form. Objects of this type carry no state that will be
   * preserved when serialized.
   * @param {!Function} constructor The type of the object that can be
   *     converted. This constructor will be called with no arguments to
   *     construct new objects.
   * @param {string} name Name of the type used in the serializable object.
   */
  static registerStatelessSerializableSpan(constructor, name) {
    const obj = {name, toJson: undefined};
    /**
     * @param {!Object} obj
     * @return {!Object}
     */
    obj.fromJson = function(obj) {
      return new constructor();
    };
    serializableSpansByName.set(name, obj);
    serializableSpansByConstructor.set(constructor, obj);
  }
}


/**
 * A spannable that allows a span value to annotate discontinuous regions of the
 * string. In effect, a span value can be set multiple times.
 * Note that most methods that assume a span value is unique such as
 * |getSpanStart| will use the first span value.
 */
export class MultiSpannable extends Spannable {
  /**
   * @param {string|!Spannable=} opt_string Initial value of the spannable.
   * @param {*=} opt_annotation Initial annotation for the entire string.
   */
  constructor(opt_string, opt_annotation) {
    super(opt_string, opt_annotation);
  }

  /** @override */
  setSpan(value, start, end) {
    this.setSpanInternal(value, start, end);
  }

  /** @override */
  substring(start, opt_end) {
    const ret = Spannable.prototype.substring.call(this, start, opt_end);
    return new MultiSpannable(ret);
    }
}


/**
 * An annotation with its start and end points.
 * @typedef {{value: *, start: number, end: number}}
 */
let SpanStruct;

/**
 * Describes how to convert a span type to/from serializable json.
 * @typedef {{name: string,
 *            fromJson: function(!Object): !Object,
 *            toJson: ((function(): !Object)|undefined)}}
 */
let SerializeInfo;

/**
 * The serialized format of a spannable.
 * @typedef {{string: string, spans: Array<SerializedSpan>}}
 * @private
 */
let SerializedSpannable;

/**
 * The format of a single annotation in a serialized spannable.
 * @typedef {{type: string, value: !Object, start: number, end: number}}
 */
let SerializedSpan;

/**
 * Maps type names to serialization info objects.
 * @type {Map<string, SerializeInfo>}
 */
const serializableSpansByName = new Map();

/**
 * Maps constructors to serialization info objects.
 * @type {Map<Function, SerializeInfo>}
 */
const serializableSpansByConstructor = new Map();

// Helpers for implementing the various |get*| methods of |Spannable|.

/**
 * @param {Function} constructor
 * @return {function(SpanStruct): boolean}
 */
function spanInstanceOf(constructor) {
  return function(span) {
    return span.value instanceof constructor;
  };
}

/**
 * @param {number} position
 * @return {function(SpanStruct): boolean}
 */
function spanCoversPosition(position) {
  return function(span) {
    return span.start <= position && position < span.end;
  };
}

/**
 * @param {*} value
 * @return {function(SpanStruct): boolean}
 */
function spanValueIs(value) {
  return function(span) {
    return span.value === value;
  };
}

/**
 * @param {!SpanStruct|undefined} span
 * @return {*}
 */
function valueOfSpan(span) {
  return span ? span.value : undefined;
}
