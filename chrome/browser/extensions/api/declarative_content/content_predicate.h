// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_PREDICATE_H_
#define CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_PREDICATE_H_

#include <memory>
#include <string>

namespace base {
class Value;
}

namespace extensions {

class ContentPredicateEvaluator;
class Extension;

// Represents a predicate that is part of a declarative rule condition in the
// Declarative Content API. This is created and can be evaluated by its
// associated ContentPredicateEvaluator subclass.
//
// For example, given the sample code at
// https://developer.chrome.com/extensions/declarativeContent#rules, the
// entities { hostEquals: 'www.google.com', schemes: ['https'] } and
// ["input[type='password']"] are both represented by ContentPredicate
// subclasses.
class ContentPredicate {
 public:
  ContentPredicate(const ContentPredicate&) = delete;
  ContentPredicate& operator=(const ContentPredicate&) = delete;

  virtual ~ContentPredicate();

  // Returns true if this predicate should be ignored during evaluation. By
  // default predicates are not ignored.
  virtual bool IsIgnored() const;

  // Returns the evaluator associated with this predicate.
  virtual ContentPredicateEvaluator* GetEvaluator() const = 0;

 protected:
  ContentPredicate();
};

// Defines the interface for objects that create predicates.
//
// Given the sample code at
// https://developer.chrome.com/extensions/declarativeContent#rules,
// ContentPredicateFactories are directly responsible for creating individual
// predicates from the { hostEquals: 'www.google.com', schemes: ['https'] } and
// ["input[type='password']"] JSON entities encoded in |value|.
class ContentPredicateFactory {
 public:
  ContentPredicateFactory(const ContentPredicateFactory&) = delete;
  ContentPredicateFactory& operator=(const ContentPredicateFactory&) = delete;

  virtual ~ContentPredicateFactory();

  // Creates a new predicate from |value|, as specified in the declarative
  // API. Sets *|error| and returns null if creation failed for any reason.
  virtual std::unique_ptr<const ContentPredicate> CreatePredicate(
      const Extension* extension,
      const base::Value& value,
      std::string* error) = 0;

 protected:
  ContentPredicateFactory();
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_DECLARATIVE_CONTENT_CONTENT_PREDICATE_H_
