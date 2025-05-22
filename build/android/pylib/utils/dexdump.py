# Copyright 2016 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import shutil
import sys
import tempfile
from xml.etree import ElementTree
from collections import namedtuple
from typing import Dict

from devil.utils import cmd_helper
from pylib import constants

sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..', 'gyp'))
from util import build_utils

DEXDUMP_PATH = os.path.join(constants.ANDROID_SDK_TOOLS, 'dexdump')


# Annotations dict format:
#   {
#     'empty-annotation-class-name': None,
#     'annotation-class-name': {
#       'fieldA': 'primitive-value',
#       'fieldB': [ 'array-item-1', 'array-item-2', ... ],
#       'fieldC': {  # CURRENTLY UNSUPPORTED.
#         /* Object value */
#         'field': 'primitive-value',
#         'field': [ 'array-item-1', 'array-item-2', ... ],
#         'field': { /* Object value */ }
#       }
#     }
#   }
Annotations = namedtuple('Annotations',
                         ['classAnnotations', 'methodsAnnotations'])

# Finds each space-separated "foo=..." (where ... can contain spaces).
_ANNOTATION_VALUE_MATCHER = re.compile(r'\w+=.*?(?:$|(?= \w+=))')


def Dump(apk_path):
  """Dumps class and method information from a APK into a dict via dexdump.

  Args:
    apk_path: An absolute path to an APK file to dump.
  Returns:
    A dict in the following format:
      {
        <package_name>: {
          'classes': {
            <class_name>: {
              'methods': [<method_1>, <method_2>],
              'superclass': <string>,
              'is_abstract': <boolean>,
              'annotations': <Annotations>
            }
          }
        }
      }
  """
  try:
    dexfile_dir = tempfile.mkdtemp()
    parsed_dex_files = []
    for dex_file in build_utils.ExtractAll(apk_path,
                                           dexfile_dir,
                                           pattern='*classes*.dex'):
      output_xml = cmd_helper.GetCmdOutput(
          [DEXDUMP_PATH, '-a', '-j', '-l', 'xml', dex_file])
      # Dexdump doesn't escape its XML output very well; decode it as utf-8 with
      # invalid sequences replaced, then remove forbidden characters and
      # re-encode it (as etree expects a byte string as input so it can figure
      # out the encoding itself from the XML declaration)
      BAD_XML_CHARS = re.compile(
          u'[\x00-\x08\x0b-\x0c\x0e-\x1f\x7f-\x84\x86-\x9f' +
          u'\ud800-\udfff\ufdd0-\ufddf\ufffe-\uffff]')

      # Line duplicated to avoid pylint redefined-variable-type error.
      clean_xml = BAD_XML_CHARS.sub(u'\ufffd', output_xml)

      # Constructors are referenced as "<init>" in our annotations
      # which will result in in the ElementTree failing to parse
      # our xml as it won't find a closing tag for this
      clean_xml = clean_xml.replace('<init>', 'constructor')

      annotations = _ParseAnnotations(clean_xml)

      parsed_dex_files.append(
          _ParseRootNode(ElementTree.fromstring(clean_xml.encode('utf-8')),
                         annotations))
    return parsed_dex_files
  finally:
    shutil.rmtree(dexfile_dir)


def _ParseAnnotationValues(values_str):
  if not values_str:
    return None
  ret = {}
  for key_value in _ANNOTATION_VALUE_MATCHER.findall(values_str):
    key, value_str = key_value.split('=', 1)
    # TODO: support for dicts if ever needed.
    if value_str.startswith('{ ') and value_str.endswith(' }'):
      value = value_str[2:-2].split()
    else:
      value = value_str
    ret[key] = value
  return ret


def _ParseAnnotations(dexRaw: str) -> Dict[int, Annotations]:
  """ Parse XML strings and return a list of Annotations mapped to
  classes by index.

  Annotations are written to the dex dump as human readable blocks of text
  The only prescription is that they appear before the class in our xml file
  They are not required to be nested within the package as our classes
  It is simpler to parse for all the annotations and then associate them
  back to the
  classes

  Example:
  Class #12 annotations:
  Annotations on class
    VISIBILITY_RUNTIME Ldalvik/annotation/EnclosingClass; value=...
  Annotations on method #512 'example'
    VISIBILITY_SYSTEM Ldalvik/annotation/Signature; value=...
    VISIBILITY_RUNTIME Landroidx/test/filters/SmallTest;
    VISIBILITY_RUNTIME Lorg/chromium/base/test/util/Feature; value={ Cronet }
    VISIBILITY_RUNTIME LFoo; key1={ A B } key2=4104 key3=null
  """

  # We want to find the lines matching the annotations header pattern
  # Eg: Class #12 annotations -> true
  annotationsBlockMatcher = re.compile(u'^Class #.*annotations:$')
  # We want to retrieve the index of the class
  # Eg: Class #12 annotations -> 12
  classIndexMatcher = re.compile(u'(?<=#)[0-9]*')
  # We want to retrieve the method name from between the quotes
  # of the annotations line
  # Eg: Annotations on method #512 'example'  -> example
  methodMatcher = re.compile(u"(?<=')[^']*")
  # We want to match everything after the last slash until before the semi colon
  # Eg: Ldalvik/annotation/Signature; -> Signature
  annotationMatcher = re.compile(u'([^/]+); ?(.*)?')

  annotations = {}
  currentAnnotationsForClass = None
  currentAnnotationsBlock: Dict[str, None] = None

  # This loop does four things
  # 1. It looks for a line telling us we are describing annotations for
  #  a new class
  # 2. It looks for a line telling us if the annotations we find will be
  #  for the class or for any of it's methods; we will keep reference to
  #  this
  # 3. It adds the annotations to whatever we are holding reference to
  # 4. It looks for a line to see if we should start looking for a
  #  new class again
  for line in dexRaw.splitlines():
    if currentAnnotationsForClass is None:
      # Step 1
      # We keep searching until we find an annotation descriptor
      # This lets us know that we are storing annotations for a new class
      if annotationsBlockMatcher.match(line):
        currentClassIndex = int(classIndexMatcher.findall(line)[0])
        currentAnnotationsForClass = Annotations(classAnnotations={},
                                                 methodsAnnotations={})
        annotations[currentClassIndex] = currentAnnotationsForClass
    else:
      # Step 2
      # If we find a descriptor indicating we are tracking annotations
      # for the class or it's methods, we'll keep a reference of this
      # block for when we start finding annotation references
      if line.startswith(u'Annotations on class'):
        currentAnnotationsBlock = currentAnnotationsForClass.classAnnotations
      elif line.startswith(u'Annotations on method'):
        method = methodMatcher.findall(line)[0]
        currentAnnotationsBlock = {}
        currentAnnotationsForClass.methodsAnnotations[
            method] = currentAnnotationsBlock

      # If we match against any other type of annotations
      # we will ignore them
      elif line.startswith(u'Annotations on'):
        currentAnnotationsBlock = None

      # Step 3
      # We are only adding runtime annotations as those are the types
      # that will affect if we should run tests or not (where this is
      # being used)
      elif currentAnnotationsBlock is not None and line.strip().startswith(
          'VISIBILITY_RUNTIME'):
        annotationName, annotationValuesStr = annotationMatcher.findall(line)[0]
        annotationValues = _ParseAnnotationValues(annotationValuesStr)

        # Our instrumentation tests expect a mapping of "Annotation: Value"
        # We aren't using the value for anything and this would increase
        # the complexity of this parser so just mapping these to None
        currentAnnotationsBlock.update({annotationName: annotationValues})

      # Step 4
      # Empty lines indicate that the annotation descriptions are complete
      # and we should look for new classes
      elif not line.strip():
        currentAnnotationsForClass = None
        currentAnnotationsBlock = None

  return annotations


def _ParseRootNode(root, annotations: Dict[int, Annotations]):
  """Parses the XML output of dexdump. This output is in the following format.

  This is a subset of the information contained within dexdump output.

  <api>
    <package name="foo.bar">
      <class name="Class" extends="foo.bar.SuperClass">
        <field name="Field">
        </field>
        <constructor name="Method">
          <parameter name="Param" type="int">
          </parameter>
        </constructor>
        <method name="Method">
          <parameter name="Param" type="int">
          </parameter>
        </method>
      </class>
    </package>
  </api>
  """
  results = {}

  # Annotations are referenced by the class order
  # To match them, we need to keep track of the class number and
  # match it to the appropriate annotation at that stage
  classCount = 0

  for child in root:
    if child.tag == 'package':
      package_name = child.attrib['name']
      parsed_node, classCount = _ParsePackageNode(child, classCount,
                                                  annotations)
      if package_name in results:
        results[package_name]['classes'].update(parsed_node['classes'])
      else:
        results[package_name] = parsed_node
  return results


def _ParsePackageNode(package_node, classCount: int,
                      annotations: Dict[int, Annotations]):
  """Parses a <package> node from the dexdump xml output.

  Returns:
    A tuple in the format:
      (classes: {
        'classes': {
          <class_1>: {
            'methods': [<method_1>, <method_2>],
            'superclass': <string>,
            'is_abstract': <boolean>,
            'annotations': <Annotations or None>
          },
          <class_2>: {
            'methods': [<method_1>, <method_2>],
            'superclass': <string>,
            'is_abstract': <boolean>,
            'annotations': <Annotations or None>
          },
        }
      }, classCount: number)
  """
  classes = {}
  for child in package_node:
    if child.tag == 'class':
      classes[child.attrib['name']] = _ParseClassNode(child, classCount,
                                                      annotations)
      classCount += 1
  return ({'classes': classes}, classCount)


def _ParseClassNode(class_node, classIndex: int,
                    annotations: Dict[int, Annotations]):
  """Parses a <class> node from the dexdump xml output.

  Returns:
    A dict in the format:
      {
        'methods': [<method_1>, <method_2>],
        'superclass': <string>,
        'is_abstract': <boolean>
      }
  """
  methods = []
  for child in class_node:
    if child.tag == 'method' and child.attrib['visibility'] == 'public':
      methods.append(child.attrib['name'])
  return {
      'methods':
      methods,
      'superclass':
      class_node.attrib['extends'],
      'is_abstract':
      class_node.attrib.get('abstract') == 'true',
      'annotations':
      annotations.get(classIndex,
                      Annotations(classAnnotations={}, methodsAnnotations={}))
  }
