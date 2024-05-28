// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Class which allows construction of annotated strings.
 */
import {TestImportManager} from '/common/testing/test_import_manager.js';

type Annotation = any;

/** The serialized format of a spannable. */
export interface SerializedSpannable {
  string: string;
  spans: SerializedSpan[];
}

export class Spannable {
  /** Underlying string. */
  private string_: string;
  /** Spans (annotations). */
  private spans_: SpanStruct[] = [];

  /**
   * @param stringValue Initial value of the spannable.
   * @param annotation Initial annotation for the entire string.
   */
  constructor(stringValue?: string|Spannable, annotation?: Annotation) {
    this.string_ = stringValue instanceof Spannable ? '' : stringValue || '';

    // Append the initial spannable.
    if (stringValue instanceof Spannable) {
      this.append(stringValue);
    }

    // Optionally annotate the entire string.
    if (annotation !== undefined) {
      const len = this.string_.length;
      this.spans_.push({value: annotation, start: 0, end: len});
    }
  }

  toString(): string {
    return this.string_;
  }

  /** @return The length of the string */
  get length(): number {
    return this.string_.length;
  }

  /**
   * Adds a span to some region of the string.
   * @param value Annotation.
   * @param start Starting index (inclusive).
   * @param end Ending index (exclusive).
   */
  setSpan(value: Annotation, start: number, end: number): void {
    this.removeSpan(value);
    this.setSpanInternal(value, start, end);
  }

  /**
   * @param value Annotation.
   * @param start Starting index (inclusive).
   * @param end Ending index (exclusive).
   */
  protected setSpanInternal(
      value: Annotation, start: number, end: number): void {
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
   * @param value Annotation.
   */
  removeSpan(value: Annotation): void {
    for (let i = this.spans_.length - 1; i >= 0; i--) {
      if (this.spans_[i].value === value) {
        this.spans_.splice(i, 1);
      }
    }
  }

  /**
   * Appends another Spannable or string to this one.
   * @param other String or spannable to concatenate.
   */
  append(other: string | Spannable): void {
    if (other instanceof Spannable) {
      const otherSpannable = other as Spannable;
      const originalLength = this.length;
      this.string_ += otherSpannable.string_;
      other.spans_.forEach(
          span => this.setSpan(
              span.value, span.start + originalLength,
              span.end + originalLength));
    } else if (typeof other === 'string') {
      this.string_ += other;
    }
  }

  /**
   * Returns the first value matching a position.
   * @param position Position to query.
   * @return Value annotating that position, or undefined if none is
   *     found.
   */
  getSpan(position: number): Annotation {
    return valueOfSpan(this.spans_.find(spanCoversPosition(position)));
  }

  /**
   * Returns the first span value which is an instance of a given constructor.
   * @param constructor Constructor.
   * @return Object if found; undefined otherwise.
   */
  getSpanInstanceOf(constructor: Function): Annotation {
    return valueOfSpan(this.spans_.find(spanInstanceOf(constructor)));
  }

  /**
   * Returns all span values which are an instance of a given constructor.
   * Spans are returned in the order of their starting index and ending index
   * for spans with equals tarting indices.
   * @param constructor Constructor.
   * @return Array of object.
   */
  getSpansInstanceOf(constructor: Function): Annotation[] {
    return (this.spans_.filter(spanInstanceOf(constructor)).map(valueOfSpan));
  }

  /**
   * Returns all spans matching a position.
   * @param position Position to query.
   * @return Values annotating that position.
   */
  getSpans(position: number): Annotation[] {
    return (this.spans_.filter(spanCoversPosition(position)).map(valueOfSpan));
  }

  /**
   * Returns whether a span is contained in this object.
   * @param value Annotation.
   */
  hasSpan(value: Annotation): boolean {
    return this.spans_.some(spanValueIs(value));
  }

  /**
   * Returns the start of the requested span. Throws if the span doesn't exist
   * in this object.
   * @param value Annotation.
   */
  getSpanStart(value: Annotation): number {
    return this.getSpanByValueOrThrow_(value).start;
  }

  /**
   * Returns the end of the requested span. Throws if the span doesn't exist
   * in this object.
   * @param value Annotation.
   */
  getSpanEnd(value: Annotation): number {
    return this.getSpanByValueOrThrow_(value).end;
  }

  /**
   * @param value Annotation.
   */
  getSpanIntervals(value: Annotation): Interval[] {
    return this.spans_.filter(span => span.value === value).map(span => {
      return {start: span.start, end: span.end};
    });
  }

  /**
   * Returns the number of characters covered by the given span. Throws if
   * the span is not in this object.
   */
  getSpanLength(value: Annotation): number {
    const span = this.getSpanByValueOrThrow_(value);
    return span.end - span.start;
  }

  /**
   * Gets the internal object for a span or throws if the span doesn't exist.
   * @param value The annotation.
   */
  private getSpanByValueOrThrow_(value: Annotation): SpanStruct {
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
   * @param start Start index, inclusive.
   * @param end End index, exclusive.
   *     If excluded, the length of the string is used instead.
   * @return Substring requested.
   */
  substring(start: number, end?: number): Spannable {
    end = end !== undefined ? end : this.string_.length;

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
   * @return String with whitespace removed.
   */
  trimLeft(): Spannable {
    return this.trim_(true, false);
  }

  /**
   * Trims whitespace from the end.
   * @return String with whitespace removed.
   */
  trimRight(): Spannable {
    return this.trim_(false, true);
  }

  /**
   * Trims whitespace from the beginning and end.
   * @return String with whitespace removed.
   */
  trim(): Spannable {
    return this.trim_(true, true);
  }

  /**
   * Trims whitespace from either the beginning and end or both.
   * @param trimStart Trims whitespace from the start of a string.
   * @param trimEnd Trims whitespace from the end of a string.
   * @return String with whitespace removed.
   */
  private trim_(trimStart: boolean, trimEnd: boolean): Spannable {
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
    // TODO(b/314203187): Not null asserted, check that this is correct.
    const trimmedStart = trimStart ? this.string_.match(/^\s*/)![0].length : 0;
    const trimmedEnd =
        trimEnd ? this.string_.match(/\s*$/)!.index : this.string_.length;
    return this.substring(trimmedStart, trimmedEnd);
  }

  /**
   * Returns this spannable to a json serializable form, including the text
   * and span objects whose types have been registered with
   * registerSerializableSpan or registerStatelessSerializableSpan.
   * @return the json serializable form.
   */
  toJson(): SerializedSpannable {
    const spans: SerializedSpan[] = [];
    this.spans_.forEach(span => {
      const serializeInfo =
          serializableSpansByConstructor.get(span.value.constructor);
      if (serializeInfo) {
        const spanObj: SerializedSpan = {
          type: serializeInfo.name,
          start: span.start,
          end: span.end,
          value: undefined,
        };
        if (serializeInfo.toJson) {
          spanObj.value = serializeInfo.toJson.apply(span.value);
        }
        spans.push(spanObj);
      }
    });
    return {string: this.string_, spans};
  }

  /**
   * Creates a spannable from a json serializable representation.
   * @param obj object containing the serializable representation.
   */
  static fromJson(obj: SerializedSpannable): Spannable {
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
      // TODO(b/314203187): Not null asserted, check that this is correct.
      const serializeInfo = serializableSpansByName.get(span.type)!;
      const value = serializeInfo.fromJson(span.value);
      return {value, start: span.start, end: span.end};
    });
    return result;
  }

  /**
   * Registers a type that can be converted to a json serializable format.
   * @param constructor The type of object that can be converted.
   * @param name String identifier used in the serializable format.
   * @param fromJson A function that converts the serializable object to an
   *     actual object of this type.
   * @param toJson A function that converts this object to a json serializable
   *     object. The function will be called with |this| set to the object to
   *     convert.
   */
  static registerSerializableSpan(
      constructor: Function, name: string,
      fromJson: (json: SerializedSpan) => Annotation,
      toJson: () => SerializedSpan): void {
    const obj: SerializeInfo = {name, fromJson, toJson};
    serializableSpansByName.set(name, obj);
    serializableSpansByConstructor.set(constructor, obj);
  }

  /**
   * Registers an object type that can be converted to/from a json
   * serializable form. Objects of this type carry no state that will be
   * preserved when serialized.
   * @param constructor The type of the object that can be converted. This
   *     constructor will be called with no arguments to construct new objects.
   * @param name Name of the type used in the serializable object.
   */
  static registerStatelessSerializableSpan(
      constructor: Function, name: string): void {
    const fromJson = function(_obj: SerializedSpan): Annotation {
      return new (constructor as FunctionConstructor)();
    };
    const obj: SerializeInfo = {name, toJson: undefined, fromJson};
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
   * @param string Initial value of the spannable.
   * @param annotation Initial annotation for the entire string.
   */
  constructor(string?: string | Spannable, annotation?: Annotation) {
    super(string, annotation);
  }

  override setSpan(value: Annotation, start: number, end: number): void {
    this.setSpanInternal(value, start, end);
  }

  override substring(start: number, end: number): MultiSpannable {
    const ret = Spannable.prototype.substring.call(this, start, end);
    return new MultiSpannable(ret);
    }
}

// Local to module.

interface Interval {
  start: number;
  end: number;
}

/** An annotation with its start and end points. */
interface SpanStruct {
  value: Annotation;
  start: number;
  end: number;
}

/** Describes how to convert a span type to/from serializable json. */
interface SerializeInfo {
  name: string;
  fromJson: (json: SerializedSpan) => Annotation;
  toJson?: () => SerializedSpan;
}

/** The format of a single annotation in a serialized spannable. */
interface SerializedSpan {
  type: string;
  value: Annotation;
  start: number;
  end: number;
}

type SpanPredicate = (span: SpanStruct) => boolean;

/** Maps type names to serialization info objects. */
const serializableSpansByName: Map<string, SerializeInfo> = new Map();

/** Maps constructors to serialization info objects. */
const serializableSpansByConstructor: Map<Function, SerializeInfo> = new Map();

// Helpers for implementing the various |get*| methods of |Spannable|.

function spanInstanceOf(constructor: Function): SpanPredicate {
  return function(span) {
    return span.value instanceof constructor;
  };
}

function spanCoversPosition(position: number): SpanPredicate {
  return function(span) {
    return span.start <= position && position < span.end;
  };
}

function spanValueIs(value: Annotation): SpanPredicate {
  return function(span) {
    return span.value === value;
  };
}

function valueOfSpan(span?: SpanStruct): Annotation {
  return span ? span.value : undefined;
}

TestImportManager.exportForTesting(Spannable, MultiSpannable);
