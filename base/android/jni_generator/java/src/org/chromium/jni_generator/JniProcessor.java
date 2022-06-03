// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.jni_generator;

import com.google.auto.service.AutoService;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Lists;
import com.google.common.collect.Maps;
import com.squareup.javapoet.AnnotationSpec;
import com.squareup.javapoet.ArrayTypeName;
import com.squareup.javapoet.ClassName;
import com.squareup.javapoet.FieldSpec;
import com.squareup.javapoet.JavaFile;
import com.squareup.javapoet.MethodSpec;
import com.squareup.javapoet.ParameterSpec;
import com.squareup.javapoet.ParameterizedTypeName;
import com.squareup.javapoet.TypeName;
import com.squareup.javapoet.TypeSpec;

import org.chromium.base.JniStaticTestMocker;
import org.chromium.base.NativeLibraryLoadedStatus;
import org.chromium.base.annotations.CheckDiscard;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.annotation.Generated;
import javax.annotation.processing.AbstractProcessor;
import javax.annotation.processing.Processor;
import javax.annotation.processing.RoundEnvironment;
import javax.annotation.processing.SupportedOptions;
import javax.lang.model.SourceVersion;
import javax.lang.model.element.Element;
import javax.lang.model.element.ElementKind;
import javax.lang.model.element.ExecutableElement;
import javax.lang.model.element.Modifier;
import javax.lang.model.element.TypeElement;
import javax.lang.model.element.VariableElement;
import javax.lang.model.type.ArrayType;
import javax.lang.model.type.TypeKind;
import javax.lang.model.type.TypeMirror;
import javax.tools.Diagnostic;

/**
 * Annotation processor that finds inner interfaces annotated with
 * {@link NativeMethods} and generates a class with native bindings
 * (GEN_JNI) and a class specific wrapper class with name (classnameJni)
 *
 * NativeClass - refers to the class that contains all native declarations.
 * NativeWrapperClass - refers to the class that is generated for each class
 * containing an interface annotated with NativeMethods.
 *
 */
@SupportedOptions({JniProcessor.SKIP_GEN_JNI_ARG})
@AutoService(Processor.class)
public class JniProcessor extends AbstractProcessor {
    static final String SKIP_GEN_JNI_ARG = "org.chromium.chrome.skipGenJni";
    private static final Class<NativeMethods> JNI_STATIC_NATIVES_CLASS = NativeMethods.class;
    private static final Class<MainDex> MAIN_DEX_CLASS = MainDex.class;
    private static final Class<CheckDiscard> CHECK_DISCARD_CLASS = CheckDiscard.class;

    private static final String CHECK_DISCARD_CRBUG = "crbug.com/993421";
    private static final String NATIVE_WRAPPER_CLASS_POSTFIX = "Jni";

    private static final ClassName GEN_JNI_CLASS_NAME =
            ClassName.get("org.chromium.base.natives", "GEN_JNI");
    private static final ClassName JNI_STATUS_CLASS_NAME =
            ClassName.get(NativeLibraryLoadedStatus.class);

    static final String NATIVE_TEST_FIELD_NAME = "TESTING_ENABLED";
    static final String NATIVE_REQUIRE_MOCK_FIELD_NAME = "REQUIRE_MOCK";

    // Builder for NativeClass which will hold all our native method declarations.
    private TypeSpec.Builder mNativesBuilder;

    // Types that are non-primitives and should not be
    // casted to objects in native method declarations.
    static final ImmutableSet JNI_OBJECT_TYPE_EXCEPTIONS =
            ImmutableSet.of("java.lang.String", "java.lang.Throwable", "java.lang.Class", "void");

    static String getNameOfWrapperClass(String containingClassName) {
        return containingClassName + NATIVE_WRAPPER_CLASS_POSTFIX;
    }

    @Override
    public Set<String> getSupportedAnnotationTypes() {
        return ImmutableSet.of(JNI_STATIC_NATIVES_CLASS.getCanonicalName());
    }

    @Override
    public SourceVersion getSupportedSourceVersion() {
        return SourceVersion.latestSupported();
    }

    public JniProcessor() {
        FieldSpec.Builder testingFlagBuilder =
                FieldSpec.builder(TypeName.BOOLEAN, NATIVE_TEST_FIELD_NAME)
                        .addModifiers(Modifier.STATIC, Modifier.PUBLIC);
        FieldSpec.Builder throwFlagBuilder =
                FieldSpec.builder(TypeName.BOOLEAN, NATIVE_REQUIRE_MOCK_FIELD_NAME)
                        .addModifiers(Modifier.STATIC, Modifier.PUBLIC);

        // State of mNativesBuilder needs to be preserved between processing rounds.
        mNativesBuilder = TypeSpec.classBuilder(GEN_JNI_CLASS_NAME)
                                  .addAnnotation(createAnnotationWithValue(
                                          Generated.class, JniProcessor.class.getCanonicalName()))
                                  .addModifiers(Modifier.PUBLIC, Modifier.FINAL)
                                  .addField(testingFlagBuilder.build())
                                  .addField(throwFlagBuilder.build());
    }

    /**
     * Processes annotations that match getSupportedAnnotationTypes()
     * Called each 'round' of annotation processing, must fail gracefully if set is empty.
     */
    @Override
    public boolean process(
            Set<? extends TypeElement> annotations, RoundEnvironment roundEnvironment) {
        // Do nothing on an empty round.
        if (annotations.isEmpty()) {
            return true;
        }

        List<JavaFile> writeQueue = Lists.newArrayList();
        for (Element e : roundEnvironment.getElementsAnnotatedWith(JNI_STATIC_NATIVES_CLASS)) {
            // @NativeMethods can only annotate types so this is safe.
            TypeElement type = (TypeElement) e;

            if (!e.getKind().isInterface()) {
                printError("@NativeMethods must annotate an interface", e);
            }

            // Interface must be nested within a class.
            Element outerElement = e.getEnclosingElement();
            if (!(outerElement instanceof TypeElement)) {
                printError(
                        "Interface annotated with @JNIInterface must be nested within a class", e);
            }

            TypeElement outerType = (TypeElement) outerElement;
            ClassName outerTypeName = ClassName.get(outerType);
            String outerClassName = outerTypeName.simpleName();
            String packageName = outerTypeName.packageName();

            // Get all methods in annotated interface.
            List<ExecutableElement> interfaceMethods = getMethodsFromType(type);

            // Map from the current method names to the method spec for a static native
            // method that will be in our big NativeClass.
            // Collisions are not allowed - no overloading.
            Map<String, MethodSpec> methodMap =
                    createNativeMethodSpecs(interfaceMethods, outerTypeName);

            // Add these to our NativeClass.
            for (MethodSpec method : methodMap.values()) {
                mNativesBuilder.addMethod(method);
            }

            // Generate a NativeWrapperClass for outerType by implementing the
            // annotated interface. Need to pass it the method map because each
            // method overridden will be a wrapper that calls its
            // native counterpart in NativeClass.
            boolean isNativesInterfacePublic = type.getModifiers().contains(Modifier.PUBLIC);
            // If the outerType needs to be in the main dex, then the generated NativeWrapperClass
            // should also be added to the main dex.
            boolean addMainDexAnnotation = outerElement.getAnnotation(MAIN_DEX_CLASS) != null;

            TypeSpec nativeWrapperClassSpec =
                    createNativeWrapperClassSpec(getNameOfWrapperClass(outerClassName),
                            isNativesInterfacePublic, addMainDexAnnotation, type, methodMap);

            // Queue this file for writing.
            // Can't write right now because the wrapper class depends on NativeClass
            // to be written and we can't write NativeClass until all @NativeMethods
            // interfaces are processed because each will add new native methods.
            JavaFile file = JavaFile.builder(packageName, nativeWrapperClassSpec).build();
            writeQueue.add(file);
        }

        // Nothing needs to be written this round.
        if (writeQueue.size() == 0) {
            return true;
        }

        try {
            // Need to write NativeClass first because the wrapper classes
            // depend on it. This step is skipped for APK targets since the final GEN_JNI class is
            // provided elsewhere.
            if (!processingEnv.getOptions().containsKey(SKIP_GEN_JNI_ARG)) {
                JavaFile nativeClassFile =
                        JavaFile.builder(GEN_JNI_CLASS_NAME.packageName(), mNativesBuilder.build())
                                .build();

                nativeClassFile.writeTo(processingEnv.getFiler());
            }

            for (JavaFile f : writeQueue) {
                f.writeTo(processingEnv.getFiler());
            }
        } catch (Exception e) {
            processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, e.getMessage());
        }
        return true;
    }

    List<ExecutableElement> getMethodsFromType(TypeElement t) {
        List<ExecutableElement> methods = Lists.newArrayList();
        for (Element e : t.getEnclosedElements()) {
            if (e.getKind() == ElementKind.METHOD) {
                methods.add((ExecutableElement) e);
            }
        }
        return methods;
    }

    /**
     * Gets method name for methods inside of NativeClass
     */
    String getNativeMethodName(String packageName, String className, String oldMethodName) {
        // e.g. org.chromium.base.Foo_Class.bar
        // => org_chromium_base_Foo_1Class_bar()
        return String.format("%s.%s.%s", packageName, className, oldMethodName)
                .replaceAll("_", "_1")
                .replaceAll("\\.", "_");
    }

    /**
     * Creates method specs for the native methods of NativeClass given
     * the method declarations from a {@link NativeMethods} annotated interface
     * @param interfaceMethods method declarations from a {@link NativeMethods} annotated
     * interface
     * @param outerType ClassName of class that contains the annotated interface
     * @return map from old method name to new native method specification
     */
    Map<String, MethodSpec> createNativeMethodSpecs(
            List<ExecutableElement> interfaceMethods, ClassName outerType) {
        Map<String, MethodSpec> methodMap = Maps.newTreeMap();
        for (ExecutableElement m : interfaceMethods) {
            String oldMethodName = m.getSimpleName().toString();
            String newMethodName = getNativeMethodName(
                    outerType.packageName(), outerType.simpleName(), oldMethodName);
            MethodSpec.Builder builder = MethodSpec.methodBuilder(newMethodName)
                                                 .addModifiers(Modifier.PUBLIC)
                                                 .addModifiers(Modifier.FINAL)
                                                 .addModifiers(Modifier.STATIC)
                                                 .addModifiers(Modifier.NATIVE);
            builder.addJavadoc(createNativeMethodJavadocString(outerType, m));

            copyMethodParamsAndReturnType(builder, m, true);
            if (methodMap.containsKey(oldMethodName)) {
                printError("Overloading is not currently implemented with this processor ", m);
            }
            methodMap.put(oldMethodName, builder.build());
        }
        return methodMap;
    }

    /**
     * Creates a generated annotation that contains the name of this class.
     */
    static AnnotationSpec createAnnotationWithValue(Class<?> annotationClazz, String value) {
        return AnnotationSpec.builder(annotationClazz)
                .addMember("value", String.format("\"%s\"", value))
                .build();
    }

    void printError(String s) {
        processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR, s);
    }

    void printError(String s, Element e) {
        processingEnv.getMessager().printMessage(Diagnostic.Kind.ERROR,
                String.format("Error processing @NativeMethods interface: %s", s), e);
    }

    /**
     * Creates a class spec for an implementation of an {@link NativeMethods} annotated interface
     * that will wrap calls to the NativesClass which contains the actual native method
     * declarations.
     *
     * This class should contain:
     * 1. Wrappers for all GEN_JNI static native methods
     * 2. A getter that when testing is disabled, will return the native implementation and
     * when testing is enabled, will call the mock of the native implementation.
     * 3. A field that holds the testNatives instance for when testing is enabled
     * 4. A TEST_HOOKS field that implements an anonymous instance of {@link JniStaticTestMocker}
     * which will set the testNatives implementation when called in tests
     *
     * @param name name of the wrapper class.
     * @param isPublic if true, a public modifier will be added to this native wrapper.
     * @param isMainDex if true, the @MainDex annotation will be added to this native wrapper.
     * @param nativeInterface the {@link NativeMethods} annotated type that this native wrapper
     *                        will implement.
     * @param methodMap a map from the old method name to the new method spec in NativeClass.
     * */
    TypeSpec createNativeWrapperClassSpec(String name, boolean isPublic, boolean isMainDex,
            TypeElement nativeInterface, Map<String, MethodSpec> methodMap) {
        // The wrapper class builder.
        TypeName nativeInterfaceType = TypeName.get(nativeInterface.asType());
        TypeSpec.Builder builder = TypeSpec.classBuilder(name)
                                           .addSuperinterface(nativeInterfaceType)
                                           .addAnnotation(createAnnotationWithValue(Generated.class,
                                                   JniProcessor.class.getCanonicalName()));
        if (isPublic) {
            builder.addModifiers(Modifier.PUBLIC);
        }
        if (isMainDex) {
            builder.addAnnotation(MAIN_DEX_CLASS);
        }
        builder.addAnnotation(createAnnotationWithValue(CHECK_DISCARD_CLASS, CHECK_DISCARD_CRBUG));

        // Start by adding all the native method wrappers.
        for (Element enclosed : nativeInterface.getEnclosedElements()) {
            if (enclosed.getKind() != ElementKind.METHOD) {
                printError("Cannot have a non-method in a @NativeMethods annotated interface",
                        enclosed);
            }

            // ElementKind.Method is ExecutableElement so this cast is safe.
            // interfaceMethod will is the method we are overloading.
            ExecutableElement interfaceMethod = (ExecutableElement) enclosed;

            // Method in NativesClass that we'll be calling.
            MethodSpec nativesMethod = methodMap.get(interfaceMethod.getSimpleName().toString());

            // Add a matching method in this class that overrides the declaration
            // in nativeInterface. It will just call the actual natives method in
            // NativeClass.
            builder.addMethod(createNativeWrapperMethod(interfaceMethod, nativesMethod));
        }

        // Add the testInstance field.
        // Holds the test natives target if it is set.
        FieldSpec testTarget = FieldSpec.builder(nativeInterfaceType, "testInstance")
                                       .addModifiers(Modifier.PRIVATE, Modifier.STATIC)
                                       .build();
        builder.addField(testTarget);

        // Getter for target or testing instance if flag in GEN_JNI is set.
        /*
        {classname}.Natives get() {
            if (GEN_JNI.TESTING_ENABLED) {
                if (testInst != null) {
                    return testInst;
                }
                if (GEN_JNI.REQUIRE_MOCK) {
                    throw new UnsupportedOperationException($noMockExceptionString);
                }
            }
            NativeLibraryLoadedStatus.checkLoaded($isMainDex)
            return new {classname}Jni();
        }
         */
        String noMockExceptionString =
                String.format("No mock found for the native implementation for %s. "
                                + "The current configuration requires all native "
                                + "implementations to have a mock instance.",
                        nativeInterfaceType);

        MethodSpec instanceGetter =
                MethodSpec.methodBuilder("get")
                        .addModifiers(Modifier.PUBLIC, Modifier.STATIC)
                        .returns(nativeInterfaceType)
                        .beginControlFlow("if ($T.$N)", GEN_JNI_CLASS_NAME, NATIVE_TEST_FIELD_NAME)
                        .beginControlFlow("if ($N != null)", testTarget)
                        .addStatement("return $N", testTarget)
                        .endControlFlow()
                        .beginControlFlow(
                                "if ($T.$N)", GEN_JNI_CLASS_NAME, NATIVE_REQUIRE_MOCK_FIELD_NAME)
                        .addStatement("throw new UnsupportedOperationException($S)",
                                noMockExceptionString)
                        .endControlFlow()
                        .endControlFlow()
                        .addStatement("$T.$N($L)", JNI_STATUS_CLASS_NAME, "checkLoaded", isMainDex)
                        .addStatement("return new $N()", name)
                        .build();

        builder.addMethod(instanceGetter);

        // Next add TEST_HOOKS to set testInstance... should look like this:
        // JniStaticTestMocker<ClassNameJni> TEST_HOOKS = new JniStaticTestMocker<>() {
        //      @Override
        //      public static setInstanceForTesting(ClassNameJni instance) {
        //          if (!GEN_JNI.TESTING_ENABLED) {
        //              throw new RuntimeException($mocksNotEnabledExceptionString);
        //          }
        //          testInstance = instance;
        //      }
        // }
        String mocksNotEnabledExceptionString =
                "Tried to set a JNI mock when mocks aren't enabled!";
        MethodSpec testHookMockerMethod =
                MethodSpec.methodBuilder("setInstanceForTesting")
                        .addModifiers(Modifier.PUBLIC)
                        .addAnnotation(Override.class)
                        .addParameter(nativeInterfaceType, "instance")
                        .beginControlFlow("if (!$T.$N)", GEN_JNI_CLASS_NAME, NATIVE_TEST_FIELD_NAME)
                        .addStatement(
                                "throw new RuntimeException($S)", mocksNotEnabledExceptionString)
                        .endControlFlow()
                        .addStatement("$N = instance", testTarget)
                        .build();

        // Make the anonymous TEST_HOOK class.
        ParameterizedTypeName genericMockerInterface = ParameterizedTypeName.get(
                ClassName.get(JniStaticTestMocker.class), ClassName.get(nativeInterface));

        TypeSpec testHook = TypeSpec.anonymousClassBuilder("")
                                    .addSuperinterface(genericMockerInterface)
                                    .addMethod(testHookMockerMethod)
                                    .build();

        FieldSpec testHookSpec =
                FieldSpec.builder(genericMockerInterface, "TEST_HOOKS")
                        .addModifiers(Modifier.STATIC, Modifier.PUBLIC, Modifier.FINAL)
                        .initializer("$L", testHook.toString())
                        .build();

        builder.addField(testHookSpec);
        return builder.build();
    }

    /**
     * Creates a wrapper method that overrides interfaceMethod and calls staticNativeMethod.
     * @param interfaceMethod method that will be overridden in a {@link NativeMethods} annotated
     * interface.
     * @param staticNativeMethod method that will be called in NativeClass.
     */
    MethodSpec createNativeWrapperMethod(
            ExecutableElement interfaceMethod, MethodSpec staticNativeMethod) {
        // Method will have the same name and be public.
        MethodSpec.Builder builder =
                MethodSpec.methodBuilder(interfaceMethod.getSimpleName().toString())
                        .addModifiers(Modifier.PUBLIC)
                        .addAnnotation(Override.class);

        // Method will need the same params and return type as the one we're overriding.
        copyMethodParamsAndReturnType(builder, interfaceMethod);

        // Add return if method return type is not void.
        if (!interfaceMethod.getReturnType().toString().equals("void")) {
            // Also need to cast because non-primitives are Objects in NativeClass.
            builder.addCode("return ($T)", interfaceMethod.getReturnType());
        }

        // Make call to native function.
        builder.addCode("$T.$N(", GEN_JNI_CLASS_NAME, staticNativeMethod);

        // Add params to native call.
        ArrayList<String> paramNames = new ArrayList<>();
        for (VariableElement param : interfaceMethod.getParameters()) {
            paramNames.add(param.getSimpleName().toString());
        }

        builder.addCode(String.join(", ", paramNames) + ");\n");
        return builder.build();
    }

    boolean shouldDowncastToObjectForJni(TypeName t) {
        if (t.isPrimitive()) {
            return false;
        }
        // There are some non-primitives that should not be downcasted.
        return !JNI_OBJECT_TYPE_EXCEPTIONS.contains(t.toString());
    }

    TypeName toTypeName(TypeMirror t, boolean useJni) {
        if (t.getKind() == TypeKind.ARRAY) {
            return ArrayTypeName.of(toTypeName(((ArrayType) t).getComponentType(), useJni));
        }
        TypeName typeName = TypeName.get(t);
        if (useJni && shouldDowncastToObjectForJni(typeName)) {
            return TypeName.OBJECT;
        }
        return typeName;
    }

    /**
     * Since some types may decay to objects in the native method
     * this method returns a javadoc string that contains the
     * type information from the old method.
     **/
    String createNativeMethodJavadocString(ClassName outerType, ExecutableElement oldMethod) {
        ArrayList<String> docLines = new ArrayList<>();

        // Class descriptor.
        String descriptor = String.format("%s.%s.%s", outerType.packageName(),
                outerType.simpleName(), oldMethod.getSimpleName().toString());
        docLines.add(descriptor);

        // Parameters.
        for (VariableElement param : oldMethod.getParameters()) {
            TypeName paramType = TypeName.get(param.asType());
            String paramTypeName = paramType.toString();
            String name = param.getSimpleName().toString();
            docLines.add(String.format("@param %s (%s)", name, paramTypeName));
        }

        // Return type.
        docLines.add(String.format("@return (%s)", oldMethod.getReturnType().toString()));

        return String.join("\n", docLines) + "\n";
    }

    void copyMethodParamsAndReturnType(
            MethodSpec.Builder builder, ExecutableElement method, boolean useJniTypes) {
        for (VariableElement param : method.getParameters()) {
            builder.addParameter(createParamSpec(param, useJniTypes));
        }
        TypeMirror givenReturnType = method.getReturnType();
        TypeName returnType = toTypeName(givenReturnType, useJniTypes);

        builder.returns(returnType);
    }

    void copyMethodParamsAndReturnType(MethodSpec.Builder builder, ExecutableElement method) {
        copyMethodParamsAndReturnType(builder, method, false);
    }

    ParameterSpec createParamSpec(VariableElement param, boolean useJniObjects) {
        TypeName paramType = toTypeName(param.asType(), useJniObjects);
        return ParameterSpec.builder(paramType, param.getSimpleName().toString())
                .addModifiers(param.getModifiers())
                .build();
    }
}
